/*******************************************************************\

Module: Symbolic Execution

Author: Daniel Kroening, kroening@kroening.com
		Lucas Cordeiro, lcc08r@ecs.soton.ac.uk

\*******************************************************************/

#include <assert.h>
#include <iostream>

#include <std_expr.h>
#include <rename.h>
#include <expr_util.h>

#include "goto_symex.h"
#include "symex_target_equation.h"

#include <std_expr.h>
#include "../ansi-c/c_types.h"
#include <base_type.h>
#include <simplify_expr.h>
#include <bits/stl_vector.h>
#include "config.h"

/*******************************************************************\

Function: goto_symext::new_name

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::new_name(symbolt &symbol) {
    get_new_name(symbol, ns);
    new_context.add(symbol);
}

/*******************************************************************\

Function: goto_symext::claim

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::claim(
  const exprt &claim_expr,
  const std::string &msg,
  statet &state,
  unsigned node_id) {

  total_claims++;

  exprt expr = claim_expr;
  //std::cout << "before rename expr.pretty(): " << expr.pretty() << "\n";
  state.rename(expr, ns, node_id);
  //std::cout << "after rename expr.pretty(): " << expr.pretty() << "\n";

  //std::cout << "before simplify expr.pretty(): " << expr.pretty() << "\n";

  // first try simplifier on it
  if (!expr.is_false())
    do_simplify(expr);

  //std::cout << "after simplify expr.pretty(): " << expr.pretty() << "\n";

  if (expr.is_true() &&
    !options.get_bool_option("all-assertions"))
  return;

  state.guard.guard_expr(expr);

  remaining_claims++;
  target->assertion(state.guard, expr, msg, state.source);
}

/*******************************************************************\

Function: goto_symext::operator()

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::operator()() {

    throw "symex_main::goto_symex::operator() : who called me?";
}
/*******************************************************************\

Function: goto_symext::operator()

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::multi_formulas_init(const goto_functionst &goto_functions)
{

  art1 = new reachability_treet(goto_functions, ns, options);
}

/*******************************************************************\

Function: goto_symext::multi_formulas_has_more_formula

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool goto_symext::multi_formulas_has_more_formula()
{
  return art1->has_more_states();
}


/*******************************************************************\

Function: goto_symext::multi_formulas_get_next_formula

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bool goto_symext::multi_formulas_get_next_formula()
{
  static unsigned int total_formulae = 0;
  static int total_states = 0;

  target = &art1->get_cur_state()._target;
  art1->get_cur_state().execute_guard(ns, *target);
  while(!art1->is_go_next_formula())
  {
    while (!art1->is_go_next_state())
      symex_step(art1->_goto_functions, *art1);

    art1->multi_formulae_go_next_state();
    target = &art1->get_cur_state()._target;
    total_states++;
  }
  art1->_go_next_formula = false;
  total_formulae++;

//  if (art1->get_actual_CS_bound() > art1->get_CS_bound())
//    std::cout << "**** WARNING: need to increase the number of context switches" << std::endl;

  return true;
}

/*******************************************************************\

Function: goto_symext::operator()

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::operator()(const goto_functionst &goto_functions)
{

  reachability_treet art(goto_functions, ns, options);

  int total_states = 0;
  while (art.has_more_states())
  {
    total_states++;
    art.get_cur_state().execute_guard(ns, *target);
    while (!art.is_go_next_state())
    {
      symex_step(goto_functions, art);
    }

    art.go_next_state();
  }

//  if (art.get_actual_CS_bound() > art.get_CS_bound())
//    std::cout << "**** WARNING: need to increase the number of context switches" << std::endl;

}

/*******************************************************************\

Function: goto_symext::get_symbol

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

irep_idt goto_symext::get_symbol(const exprt & expr) {
  if (expr.id() != exprt::symbol) {
    forall_operands(it, expr) {
      return get_symbol(*it);
    }
  }

  return expr.identifier();
}

/*******************************************************************\

Function: goto_symext::symex_step

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void goto_symext::symex_step(
        const goto_functionst &goto_functions,
        reachability_treet & art) {

  static bool is_main=false, is_goto=false;

  execution_statet &ex_state = art.get_cur_state();
  statet &state = ex_state.get_active_state();

  assert(!state.call_stack.empty());

  const goto_programt::instructiont &instruction = *state.source.pc;

  merge_gotos(state, ex_state, ex_state.node_id);

  // depth exceeded?
  {
      unsigned max_depth = atoi(options.get_option("depth").c_str());
      if (max_depth != 0 && state.depth > max_depth)
          state.guard.add(false_exprt());
      state.depth++;
  }

  if (options.get_bool_option("symex-trace")) {
    const goto_programt p_dummy;
    goto_functions_templatet<goto_programt>::function_mapt::const_iterator it =
      goto_functions.function_map.find(instruction.function);

    const goto_programt &p_real = it->second.body;
    const goto_programt &p = (it == goto_functions.function_map.end()) ? p_dummy : p_real;
    p.output_instruction(ns, "", std::cout, state.source.pc, false, false);
  }

    // actually do instruction
    switch (instruction.type) {
        case SKIP:
            // really ignore
            state.source.pc++;
            break;
        case END_FUNCTION:
            if(instruction.function == "c::main")
            {
                ex_state.end_thread(ns, *target);
                ex_state.reexecute_instruction = false;
                art.generate_states_base(exprt());
                art.set_go_next_state();
                is_main=false;
            }
            else
            {
                symex_end_of_function(state);
                state.source.pc++;
            }
            break;
        case LOCATION:
            target->location(state.guard, state.source);
            state.source.pc++;
            break;
        case GOTO:
        {
        	is_goto=true;
            exprt tmp(instruction.guard);
            replace_dynamic_allocation(state, tmp);
            replace_nondet(tmp, ex_state);
            dereference(tmp, state, false, ex_state.node_id);

            if(!tmp.is_nil() && !options.get_bool_option("deadlock-check") /*&& is_main*/)
            {
              if(ex_state._threads_state.size() > 1)
                if (art.generate_states_before_read(tmp))
                  return;
            }

            symex_goto(art.get_cur_state().get_active_state(), ex_state, ex_state.node_id);
        }
            break;
        case ASSUME:
            if (!state.guard.is_false()) {
                exprt tmp(instruction.guard);
                replace_dynamic_allocation(state, tmp);
                replace_nondet(tmp, ex_state);
                dereference(tmp, state, false, ex_state.node_id);

                exprt tmp1 = tmp;
                state.rename(tmp, ns,ex_state.node_id);

                do_simplify(tmp);
                if (!tmp.is_true())
                {
                  if(ex_state._threads_state.size() > 1)
                    if (art.generate_states_before_read(tmp1))
                      return;

                    exprt tmp2 = tmp;
                    state.guard.guard_expr(tmp2);
                    target->assumption(state.guard, tmp2, state.source);

                    // we also add it to the state guard
                    state.guard.add(tmp);
                }
            }
            state.source.pc++;
            break;

        case ASSERT:
            if (!state.guard.is_false()) {
                if (!options.get_bool_option("no-assertions") ||
                        !state.source.pc->location.get_bool("user-provided")
                        || options.get_bool_option("deadlock-check")) {

                    std::string msg = state.source.pc->location.comment().as_string();
                    if (msg == "") msg = "assertion";
                    exprt tmp(instruction.guard);

                    replace_dynamic_allocation(state, tmp);
                    replace_nondet(tmp, ex_state);
                    dereference(tmp, state, false, ex_state.node_id);

                    if(ex_state._threads_state.size() > 1)
                      if (art.generate_states_before_read(tmp))
                        return;

                    claim(tmp, msg, state, ex_state.node_id);
                }
            }
            state.source.pc++;
            break;

        case RETURN:
        	 if(!state.guard.is_false())
                         symex_return(state, ex_state, ex_state.node_id);

            state.source.pc++;
            break;

        case ASSIGN:
            if (!state.guard.is_false()) {
                codet deref_code=instruction.code;
                replace_dynamic_allocation(state, deref_code);
                replace_nondet(deref_code, ex_state);
                assert(deref_code.operands().size()==2);

                dereference(deref_code.op0(), state, true, ex_state.node_id);
                dereference(deref_code.op1(), state, false, ex_state.node_id);

                basic_symext::symex_assign(state, ex_state, deref_code, ex_state.node_id);

                state.source.pc++;

                if(ex_state._threads_state.size() > 1)
                {
                  if (!is_goto && !is_main)
                  {
                    if (art.generate_states_before_assign(deref_code, ex_state))
                      return;
                  }
                  else
                  {
                	if (is_main && (options.get_bool_option("control-flow-test")) &&
              			  (ex_state.get_expr_write_globals(ns,deref_code.op0())>0))
                	{
                  	    ex_state.reexecute_instruction = false;
                  	    art.generate_states();
                    }
                    is_goto=false;
                  }
                }
            }
            else
              state.source.pc++;
            break;
        case FUNCTION_CALL:
            if (!state.guard.is_false())
            {
                code_function_callt deref_code =
                        to_code_function_call(instruction.code);

                replace_dynamic_allocation(state, deref_code);
                replace_nondet(deref_code, ex_state);

                if (deref_code.lhs().is_not_nil()) {
                    dereference(deref_code.lhs(), state, true,ex_state.node_id);
                }

                dereference(deref_code.function(), state, false, ex_state.node_id);

                if(deref_code.function().identifier() == "c::main")
                  is_main=true;

                if(deref_code.function().identifier() == "c::__ESBMC_yield")
                {
                   state.source.pc++;
                   ex_state.reexecute_instruction = false;
                   art.generate_states();
                   return;
                }

                if (deref_code.function().identifier() == "c::__ESBMC_switch_to")
                {
                  state.source.pc++;
                  ex_state.reexecute_instruction = false;

                  assert(deref_code.arguments().size() == 1);

                  // Switch to other thread.
                  exprt &num = deref_code.arguments()[0];
                  if (num.id() != "constant")
                    throw "Can't switch to non-constant thread id no";

                  unsigned int tid = binary2integer(num.value().as_string(), false).to_long();
                  ex_state.set_active_state(tid);
                  return;
                }

                Forall_expr(it, deref_code.arguments()) {
                    dereference(*it, state, false,ex_state.node_id);
                }

                symex_function_call(goto_functions, ex_state, deref_code);

//                ex_state.reexecute_instruction = false;
//                art.generate_states();

            }
            else
            {
                state.source.pc++;
            }
            break;

        case OTHER:
            if (!state.guard.is_false()) {
                codet deref_code(instruction.code);
                const irep_idt &statement = deref_code.get_statement();
                if (/*statement == "cpp_delete" ||
                        statement == "cpp_delete[]" ||*/
                        statement == "free" ||
                        statement == "printf") {
                    replace_dynamic_allocation(state, deref_code);
                    replace_nondet(deref_code, ex_state);
                    dereference(deref_code, state, false,ex_state.node_id);

                    //if(ex_state._threads_state.size() > 1)
                      //if (art.generate_states_before_read(deref_code))
                        //return;
                }

                symex_other(goto_functions, state, ex_state,  ex_state.node_id);
            }
            state.source.pc++;
            break;

        case SYNC:
            throw "SYNC not yet implemented";

        case START_THREAD:
        	if (!state.guard.is_false())
        	{
          	  assert(!instruction.targets.empty());
          	  goto_programt::const_targett goto_target=instruction.targets.front();

              state.source.pc++;
              ex_state.add_thread(state);
              ex_state.get_active_state().source.pc = goto_target;

              //ex_state.deadlock_detection(ns,*target);
              ex_state.update_trds_count(ns,*target);
              ex_state.increament_trds_in_run(ns,*target);

              ex_state.generating_new_threads = ex_state._threads_state.size() - 1;
            }
        	else
        	{
              assert(!instruction.targets.empty());
              goto_programt::const_targett goto_target=instruction.targets.front();
              state.source.pc = goto_target;
            }

            break;
        case END_THREAD:
            ex_state.end_thread(ns, *target);
            ex_state.reexecute_instruction = false;
            art.generate_states();
            //if (art.generate_states_base(exprt()))
              //return;
            break;
        case ATOMIC_BEGIN:
            state.source.pc++;
            ex_state.increment_active_atomic_number();
            break;
        case ATOMIC_END:
            ex_state.decrement_active_atomic_number();
            state.source.pc++;
            break;
        default:
            assert(false);
    }
}
