/*******************************************************************\

Module: C++ Language Conversion

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/

#include <assert.h>
#include <stdlib.h>

#include <arith_tools.h>
#include <config.h>
#include <ieee_float.h>
#include <std_types.h>

#include "c_types.h"
#include "parse_float.h"
#include "convert_float_literal.h"

/*******************************************************************\

Function: convert_float_literal

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void convert_float_literal(
  const std::string &src,
  exprt &dest)
{
  mp_integer significand;
  mp_integer exponent;
  bool is_float, is_long;
  
  parse_float(src, significand, exponent, is_float, is_long);
  
  dest=exprt("constant");
  
  dest.set("#cformat", src);

  if(is_float)
    dest.type()=float_type();
  else if(is_long)
    dest.type()=long_double_type();
  else
    dest.type()=double_type();

  if(config.ansi_c.use_fixed_for_float)
  {
    unsigned width=atoi(dest.type().width().c_str());
    unsigned fraction_bits;
    const std::string &integer_bits=dest.type().integer_bits().as_string();

    if(integer_bits=="")
      fraction_bits=width/2;
    else
      fraction_bits=width-atoi(integer_bits.c_str());

    mp_integer factor=mp_integer(1)<<fraction_bits;
    mp_integer value=significand*factor;

    if(exponent<0)
      value/=power(10, -exponent);
    else
      value*=power(10, exponent);

    dest.value(integer2binary(value, width));
  }
  else
  {
    ieee_floatt a;

    a.spec=to_floatbv_type(dest.type());
    a.from_base10(significand, exponent);

    dest.value(integer2binary(a.pack(), a.spec.width()));
  }
}
