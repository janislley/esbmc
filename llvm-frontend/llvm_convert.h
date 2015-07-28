/*
 * llvmtypecheck.h
 *
 *  Created on: Jul 23, 2015
 *      Author: mramalho
 */

#ifndef LLVM_FRONTEND_LLVM_CONVERT_H_
#define LLVM_FRONTEND_LLVM_CONVERT_H_

#include <context.h>
#include <std_types.h>

#include <clang/Frontend/ASTUnit.h>
#include <clang/AST/Type.h>
#include <clang/AST/Expr.h>

class llvm_convertert
{
public:
  llvm_convertert(contextt &_context);

  virtual ~llvm_convertert();

  bool convert();

  std::vector<std::unique_ptr<clang::ASTUnit> > ASTs;

private:
  contextt &context;
  locationt current_location;
  std::string current_path;

  bool convert_top_level_decl();
  void convert_typedef(clang::ASTUnit::top_level_iterator it);
  void convert_var(clang::ASTUnit::top_level_iterator it);
  void convert_function(clang::ASTUnit::top_level_iterator it);
  code_typet::argumentt convert_function_params(
    std::string function_name,
    clang::ParmVarDecl *pdecl);

  void get_type(const clang::QualType &the_type, typet &new_type);
  void get_expr(const clang::Expr &expr, exprt &new_expr);
  void get_default_symbol(symbolt &symbol);

  void update_current_location(clang::ASTUnit::top_level_iterator it);
  std::string get_filename_from_path();
  std::string get_modulename_from_path();
};

#endif /* LLVM_FRONTEND_LLVM_CONVERT_H_ */
