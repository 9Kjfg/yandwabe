struct entity;

enum brain_type
{
	Brain_Hero,

    // NOTE: Test brains
	Brain_Snake,
    Brain_Familiar,
    Brain_FloatyThingForNow,
    Brain_Monstar,

	Brain_Count,
};

struct brain_slot
{
	u32 Index;
};

struct brain_id
{
	u32 Value;
};

struct brain_hero_parts
{
	entity *Head;
	entity *Body;
};

#define BrainSlotFor(type, Member) BrainSlotFor_(&(((type *)0)->Member) - (entity **)0)
inline brain_slot
BrainSlotFor_(u32 PackValue)
{
	brain_slot Result = {PackValue};
	return(Result);
}

struct brain
{
    brain_id ID;
    brain_type Type;

	union
	{
		brain_hero_parts Hero;
		entity *Array[16];
	};
};

enum reserved_brain_id
{
	ReservedBrainID_FirstHero = 1,
	ReservedBrainID_LastHero = (ReservedBrainID_FirstHero + MAX_CONTROLLER_COUNT - 1),

	ReservedBrainID_FirstFree,
};

