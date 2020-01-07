/*
* Copyright (c) 2019,YBX is the most handsome man in NCEPU
* All rights reserved.
*
* 文件名称: create.c
* 文件标识: 见README.md
* 摘要:分析树的构建——与ybx.y一起定义  
*      定义ybx语言中的变量类型，以及如何调用C语言函数，如何注册到解释器的规则
*      面向内置函数开发人员的接口
*
* 当前版本: 1.1
* 作者: 杨秉学
* 完成日期: 2019年11月17日
*
* 取代版本:1.0
* 原作者: 杨秉学
* 完成日期: 2018年5月2日
*/

#include "MEM.h"
#include "DBG.h"
#include "ybxcompiler.h"

// 函数定义
void ybx_function_define(char *identifier, ParameterList *parameter_list,Block *block)
{
    FunctionDefinition *f;
    YBX_Interpreter *inter;

    if (ybx_search_function(identifier))												// 查看该函数是否已经定义
	{
        ybx_compile_error(FUNCTION_MULTIPLE_DEFINE_ERR,
                          STRING_MESSAGE_ARGUMENT, "name", identifier,
                          MESSAGE_ARGUMENT_END);
        return;
    }

    inter = ybx_get_current_interpreter();												// 获取当前的解释器

    f = ybx_malloc(sizeof(FunctionDefinition));
    f->name = identifier;
    f->type = YBX_FUNCTION_DEFINITION;								                // 函数类型为用户自定义
    f->u.ybx_f.parameter = parameter_list;									    	// 形参列表
    f->u.ybx_f.block = block;														// 函数体（就是语句块）
    f->next = inter->function_list;
    inter->function_list = f;
}

// 创建参数（形参）
ParameterList * ybx_create_parameter(char *identifier)
{
    ParameterList       *p;

    p = ybx_malloc(sizeof(ParameterList));
    p->name = identifier;
    p->next = NULL;

    return p;
}

// 参数链（形参，将形参构成一个形参链）
ParameterList * ybx_chain_parameter(ParameterList *list, char *identifier)
{
    ParameterList *pos;

    for (pos = list; pos->next; pos = pos->next)
        ;
    pos->next = ybx_create_parameter(identifier);

    return list;
}

// 创建参数链（实参）
ArgumentList * ybx_create_argument_list(Expression *expression)
{
    ArgumentList *al;

    al = ybx_malloc(sizeof(ArgumentList));
    al->expression = expression;
    al->next = NULL;

    return al;
}

// 参数链（实参）
ArgumentList * ybx_chain_argument_list(ArgumentList *list, Expression *expr)
{
    ArgumentList *pos;

    for (pos = list; pos->next; pos = pos->next)
        ;
    pos->next = ybx_create_argument_list(expr);

    return list;
}

// 创建语句链
StatementList * ybx_create_statement_list(Statement *statement)
{
    StatementList *sl;

    sl = ybx_malloc(sizeof(StatementList));
    sl->statement = statement;
    sl->next = NULL;

    return sl;
}

// 语句链
StatementList * ybx_chain_statement_list(StatementList *list, Statement *statement)
{
    StatementList *pos;

    if (list == NULL)
        return ybx_create_statement_list(statement);

    for (pos = list; pos->next; pos = pos->next)
        ;
    pos->next = ybx_create_statement_list(statement);

    return list;
}

// 为表达式分配内存
Expression * ybx_alloc_expression(ExpressionType type)
{
    Expression  *exp;

    exp = ybx_malloc(sizeof(Expression));
    exp->type = type;
    exp->line_number = ybx_get_current_interpreter()->current_line_number;              // 设置该表达式的行号

    return exp;
}

// 创建赋值表达式
Expression * ybx_create_assign_expression(char *variable, Expression *operand)
{
    Expression *exp;

    exp = ybx_alloc_expression(ASSIGN_EXPRESSION);
    exp->u.assign_expression.variable = variable;
    exp->u.assign_expression.operand = operand;				                            // 表达式的操作

    return exp;
}

// 将一个变量转换成表达式
static Expression convert_value_to_expression(YBX_Value *v)
{
    Expression  expr;

    if (v->type == YBX_INT_VALUE)									                    // 只有这几种变量能够转换成表达式：int、double、bool
	{
        expr.type = INT_EXPRESSION;
        expr.u.int_value = v->u.int_value;
    } 
	else if (v->type == YBX_DOUBLE_VALUE) 
	{
        expr.type = DOUBLE_EXPRESSION;
        expr.u.double_value = v->u.double_value;
    }
	else
	{
        DBG_assert(v->type == YBX_BOOLEAN_VALUE,
                   ("v->type..%d\n", v->type));
        expr.type = BOOLEAN_EXPRESSION;
        expr.u.boolean_value = v->u.boolean_value;
    }
    return expr;
}

// 创建二元表达式
Expression * ybx_create_binary_expression(ExpressionType operator,Expression *left, Expression *right)
{
    if ((left->type == INT_EXPRESSION || left->type == DOUBLE_EXPRESSION)
        && (right->type == INT_EXPRESSION || right->type == DOUBLE_EXPRESSION)) 
	{
        YBX_Value v;
        v = ybx_eval_binary_expression(ybx_get_current_interpreter(),
                                       NULL, operator, left, right);
        /* Overwriting left hand expression. */
        *left = convert_value_to_expression(&v);

        return left;
    } 
	else
	{
        Expression *exp;
        exp = ybx_alloc_expression(operator);
        exp->u.binary_expression.left = left;
        exp->u.binary_expression.right = right;
        return exp;
    }
}

// 创建 取负 表达式
Expression * ybx_create_minus_expression(Expression *operand)
{
    if (operand->type == INT_EXPRESSION
        || operand->type == DOUBLE_EXPRESSION) 
	{
        YBX_Value       v;
        v = ybx_eval_minus_expression(ybx_get_current_interpreter(),
                                      NULL, operand);
        /* Notice! Overwriting operand expression. */
        *operand = convert_value_to_expression(&v);
        return operand;
    }
	else 
	{
        Expression      *exp;
        exp = ybx_alloc_expression(MINUS_EXPRESSION);
        exp->u.minus_expression = operand;
        return exp;
    }
}

// 创建 标识符(定义) 表达式
Expression * ybx_create_identifier_expression(char *identifier)
{
    Expression  *exp;

    exp = ybx_alloc_expression(IDENTIFIER_EXPRESSION);
    exp->u.identifier = identifier;

    return exp;
}

// 创建 函数调用 表达式
Expression * ybx_create_function_call_expression(char *func_name, ArgumentList *argument)
{
    Expression  *exp;

    exp = ybx_alloc_expression(FUNCTION_CALL_EXPRESSION);
    exp->u.function_call_expression.identifier = func_name;
    exp->u.function_call_expression.argument = argument;

    return exp;
}

// 创建 布尔 表达式
Expression * ybx_create_boolean_expression(YBX_Boolean value)
{
    Expression *exp;

    exp = ybx_alloc_expression(BOOLEAN_EXPRESSION);
    exp->u.boolean_value = value;

    return exp;
}

// 创建 null 表达式
Expression * ybx_create_null_expression(void)
{
    Expression  *exp;

    exp = ybx_alloc_expression(NULL_EXPRESSION);

    return exp;
}

// 为语句分配内存
static Statement * alloc_statement(StatementType type)
{
    Statement *st;

    st = ybx_malloc(sizeof(Statement));
    st->type = type;
    st->line_number = ybx_get_current_interpreter()->current_line_number;

    return st;
}

// 创建全局的语句
Statement * ybx_create_global_statement(IdentifierList *identifier_list)
{
    Statement *st;

    st = alloc_statement(GLOBAL_STATEMENT);
    st->u.global_s.identifier_list = identifier_list;

    return st;
}

// 创建全局的标识符
IdentifierList * ybx_create_global_identifier(char *identifier)
{
    IdentifierList      *i_list;

    i_list = ybx_malloc(sizeof(IdentifierList));
    i_list->name = identifier;
    i_list->next = NULL;

    return i_list;
}

// 标识符链
IdentifierList * ybx_chain_identifier(IdentifierList *list, char *identifier)
{
    IdentifierList *pos;

    for (pos = list; pos->next; pos = pos->next)
        ;
    pos->next = ybx_create_global_identifier(identifier);

    return list;
}

// 创建if语句
Statement * ybx_create_if_statement(Expression *condition,
                        Block *then_block, Elsif *elsif_list,
                        Block *else_block)
{
    Statement *st;

    st = alloc_statement(IF_STATEMENT);
    st->u.if_s.condition = condition;
    st->u.if_s.then_block = then_block;
    st->u.if_s.elsif_list = elsif_list;
    st->u.if_s.else_block = else_block;

    return st;
}

// elsif语句链
Elsif * ybx_chain_elsif_list(Elsif *list, Elsif *add)
{
    Elsif *pos;

    for (pos = list; pos->next; pos = pos->next)
        ;
    pos->next = add;

    return list;
}

// 创建elsif语句
Elsif * ybx_create_elsif(Expression *expr, Block *block)
{
    Elsif *ei;

    ei = ybx_malloc(sizeof(Elsif));
    ei->condition = expr;
    ei->block = block;
    ei->next = NULL;

    return ei;
}

// 创建while语句
Statement * ybx_create_while_statement(Expression *condition, Block *block)
{
    Statement *st;

    st = alloc_statement(WHILE_STATEMENT);
    st->u.while_s.condition = condition;
    st->u.while_s.block = block;

    return st;
}

// 创建for语句
Statement * ybx_create_for_statement(Expression *init, Expression *cond,
                         Expression *post, Block *block)
{
    Statement *st;

    st = alloc_statement(FOR_STATEMENT);
    st->u.for_s.init = init;
    st->u.for_s.condition = cond;
    st->u.for_s.post = post;
    st->u.for_s.block = block;

    return st;
}

// 语句块（函数体或者其他的）
Block * ybx_create_block(StatementList *statement_list)
{
    Block *block;

    block = ybx_malloc(sizeof(Block));
    block->statement_list = statement_list;

    return block;
}

// 创建表达式语句（？）
Statement * ybx_create_expression_statement(Expression *expression)
{
    Statement *st;

    st = alloc_statement(EXPRESSION_STATEMENT);
    st->u.expression_s = expression;

    return st;
}

// 创建return 语句
Statement * ybx_create_return_statement(Expression *expression)
{
    Statement *st;

    st = alloc_statement(RETURN_STATEMENT);
    st->u.return_s.return_value = expression;

    return st;
}

// 创建break语句
Statement *ybx_create_break_statement(void)
{
    return alloc_statement(BREAK_STATEMENT);
}

// 创建continue语句
Statement *ybx_create_continue_statement(void)
{
    return alloc_statement(CONTINUE_STATEMENT);
}

