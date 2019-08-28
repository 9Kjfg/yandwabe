struct entity;

//
// NOTE: Brain type
//
struct brain_hero
{
	entity *Head;
	entity *Body;
};

struct brain_monstar
{
	entity *Body;
};

struct brain_familiar
{
	entity *Head;
};

//
// NOTE: Brain
//

enum brain_type
{
	Type_brain_hero,

    // NOTE: Test brains
	Type_brain_snake,
    Type_brain_familiar,
    Type_brain_floaty_thing_for_now,
    Type_brain_monstar,

	Type_brain_count,
};

struct brain_slot
{
	u16 Type;
	u16 Index;
};

#define BrainSlotFor(type, Member) BrainSlotFor_(Type_##type, &(((type *)0)->Member) - (entity **)0)
inline brain_slot
BrainSlotFor_(brain_type Type, u16 PackValue)
{
	brain_slot Result = {(u16)Type, PackValue};
	return(Result);
}

struct brain_id
{
	u32 Value;
};

struct brain
{
    brain_id ID;
    brain_type Type;

	union
	{
		brain_hero Hero;
		brain_monstar Monstar;
		brain_familiar Familiar;
		entity *Array[16];
	};
};

enum reserved_brain_id
{
	ReservedBrainID_FirstHero = 1,
	ReservedBrainID_LastHero = (ReservedBrainID_FirstHero + MAX_CONTROLLER_COUNT - 1),

	ReservedBrainID_FirstFree,
};

