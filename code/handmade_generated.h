member_definition MembersOf_rectangle2[] = 
{
    {0, MetaType_v2, "Min", PointerToU32(&((rectangle2 *)0)->Min)},
    {0, MetaType_v2, "Max", PointerToU32(&((rectangle2 *)0)->Max)},
};
member_definition MembersOf_rectangle3[] = 
{
    {0, MetaType_v3, "Min", PointerToU32(&((rectangle3 *)0)->Min)},
    {0, MetaType_v3, "Max", PointerToU32(&((rectangle3 *)0)->Max)},
};
#define META_HANDLE_TYPE_DUMP(MemberPtr, NextIndentLevel)\
    case MetaType_rectangle3: {DEBUGTextLine(Member->Name), DEBUGDumpStruct(ArrayCount(MembersOf_rectangle3), MembersOf_rectangle3, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_rectangle2: {DEBUGTextLine(Member->Name), DEBUGDumpStruct(ArrayCount(MembersOf_rectangle2), MembersOf_rectangle2, MemberPtr, (NextIndentLevel));} break; 
