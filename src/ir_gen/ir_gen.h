/* date = February 4th 2025 11:03 pm */

#ifndef IR_GEN_H
#define IR_GEN_H

typedef enum IR_NodeType
{
  IR_NodeType_Select,
  IR_NodeType_Column,
  IR_NodeType_Table,
  IR_NodeType_Database,
  IR_NodeType_Condition,
  IR_NodeType_Operator,
  IR_NodeType_Literal,
} IR_NodeType;

typedef struct IR_Node IR_Node;
struct IR_Node
{
  IR_Node* left;
  IR_Node* right;
  IR_Node* next;
  IR_NodeType type;
  String8 value;
};

typedef struct IR_Query IR_Query;
struct IR_Query
{
  IR_Node* select_columns;
  U64 select_column_count;
  IR_Node* from_table;
  IR_Node* where_conditions;
  IR_Node* database;
};

internal IR_Query* ir_generate_from_ast(Arena* arena, SQL_Node* ast_root);
internal IR_Node* ir_convert_expression(Arena* arena, SQL_Node *ast_expr);

internal void ir_print_node(IR_Node *node, U64 depth);
internal void ir_print_query(IR_Query *query);

#endif //IR_GEN_H
