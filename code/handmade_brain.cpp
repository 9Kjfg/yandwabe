inline void
ExecuteBrain(game_state *GameState, game_input *Input,
    sim_region *SimRegion, brain *Brain, r32 dt)
{
    switch (Brain->Type)
    {
        case Type_brain_hero:
        {
            // TODO: Check that they're not deleted when we do
            brain_hero *Parts = &Brain->Hero;
            entity *Head = Parts->Head;
            entity *Body = Parts->Body;

            u32 ControllerIndex = Brain->ID.Value - ReservedBrainID_FirstHero;
            game_controller_input *Controller = GetController(Input, ControllerIndex);
            controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;

            v2 dSword = {};
            r32 dZ = 0.0f;
            b32 Exited = false;
            b32 DebugSpawn = false;

            if (Controller->IsAnalog)
            {
                ConHero->ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
            }
            else
            {
                //NOTE: Use digital movement tuning
                r32 Recenter = 0.5f;
                if (WasPressed(Controller->MoveUp))
                {
                    ConHero->ddP.x = 0.0f;
                    ConHero->ddP.y = 1.0f;
                    ConHero->RecenterTimer = Recenter;
                }
                if (WasPressed(Controller->MoveDown))
                {
                    ConHero->ddP.x = 0.0f;
                    ConHero->ddP.y = -1.0f;
                    ConHero->RecenterTimer = Recenter;
                }
                if (WasPressed(Controller->MoveLeft))
                {
                    ConHero->ddP.x = -1.0f;
                    ConHero->ddP.y = 0.0f;
                    ConHero->RecenterTimer = Recenter;
                }
                if (WasPressed(Controller->MoveRight))
                {
                    ConHero->ddP.x = 1.0f;
                    ConHero->ddP.y = 0.0f;
                    ConHero->RecenterTimer = Recenter;
                }

                if (!IsDown(Controller->MoveLeft) &&
                    !IsDown(Controller->MoveRight))
                {
                    ConHero->ddP.x = 0.0f;
                    if (IsDown(Controller->MoveUp))
                    {
                        ConHero->ddP.y = 1.0f;
                    }
                    if (IsDown(Controller->MoveDown))
                    {
                        ConHero->ddP.y = -1.0f;
                    }
                }
                
                if (!IsDown(Controller->MoveUp) &&
                    !IsDown(Controller->MoveDown))
                {
                    ConHero->ddP.y = 0.0f;
                    if (IsDown(Controller->MoveLeft))
                    {
                        ConHero->ddP.x = -1.0f;
                    }
                    if (IsDown(Controller->MoveRight))
                    {
                        ConHero->ddP.x = 1.0f;
                    }
                }

                if (WasPressed(Controller->Start))
                {
                    DebugSpawn = true;
                }
            }

            if (Head && WasPressed(Controller->Start))
            {
                entity *ClosestHero = 0;
                r32 ClosestHeroDSq = Square(10.0f); // NOTE: Ten meter maximum search
                entity *TestEntity = SimRegion->Entities;
                for (uint32 TestEntityIndex = 0;
                    TestEntityIndex < SimRegion->EntityCount;
                    ++TestEntityIndex, ++TestEntity)
                {	
                    if ((TestEntity->BrainID.Value != Head->BrainID.Value) && (TestEntity->BrainID.Value))
                    {
                        real32 TestDSq = LengthSq(TestEntity->P - Head->P);
                        if (ClosestHeroDSq > TestDSq)
                        {
                            ClosestHero = TestEntity;
                            ClosestHeroDSq = TestDSq;
                        }
                    }
                }

                if (ClosestHero)
                {
                    brain_id OldBrainID = Head->BrainID;
                    brain_slot OldBrainSlot = Head->BrainSlot;
                    Head->BrainID = ClosestHero->BrainID;
                    Head->BrainSlot = ClosestHero->BrainSlot;
                    ClosestHero->BrainID = OldBrainID;
                    ClosestHero->BrainSlot = OldBrainSlot;
                }
            }

            dSword = {};
            if (Controller->ActionUp.EndedDown)
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 10.0f, V2(1.0f, 1.0f));
                dSword = V2(0.0f, 1.0f);
            }
            if (Controller->ActionDown.EndedDown)
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 10.0f, V2(0.0f, 0.0f));
                dSword = V2(0.0f, -1.0f);
            }
            if (Controller->ActionLeft.EndedDown)
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 5.0f, V2(0.0f, 0.0f));
                dSword = V2(-1.0f, 0.0f);
            }
            if (Controller->ActionRight.EndedDown)
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 5.0f, V2(0.0f, 0.0f));
                dSword = V2(1.0f, 0.0f);
            }

            if (WasPressed(Controller->Back))
            {
                Exited = true;
            }

#if 0
            if (ConHero->DebugSpawn && Head)
            {
                traversable_reference Traversable;
                if (GetClosestTraversable(SimRegion, Head->P, &Traversable,
                        TraversableSearch_Unoccupied))
                {
                    AddPlayer(WorldMode, SimRegion, Traversable);
                }
                else
                {
                    // TODO: GameUI that tells you there's no sae place...
                    // maybe keep trying on subsequent frames
                }

                ConHero->DebugSpawn = false;
            }
#endif			
            ConHero->RecenterTimer = ClampAboveZero(ConHero->RecenterTimer - dt);
            
            if (Head)
            {
                // TODO: Change to using accelaration vector
                if ((dSword.x == 0.0f) && (dSword.y == 0.0f))
                {
                    // NOTE: Leave FacingDirection whatever it was
                }
                else
                {
                    Head->FacingDirection = ATan2(dSword.y, dSword.x);
                }
            }

            traversable_reference Traversable;
            if (Head && GetClosestTraversable(SimRegion, Head->P, &Traversable))
            {
                if (Body)
                {
                    if (Body->MovementMode == MovementMode_Planted)
                    {
                        if (!IsEqual(Traversable, Body->Occupying))
                        {
                            Body->CameFrom = Body->Occupying;
                            if (TransactionOccupy(Body, &Body->Occupying, Traversable))
                            {
                                Body->tMovement = 0.0f;
                                Body->MovementMode = MovementMode_Hopping;
                            }
                        }
                    }
                }

                v3 ClosestP = GetSimSpaceTraversable(Traversable).P;

                b32 TimerIsUp = (ConHero->RecenterTimer == 0.0f);
                b32 NoPush = (LengthSq(ConHero->ddP) < 0.1f);
                r32 Cp = NoPush ? 300.0f : 25.0f;
                v3 ddP2 = V3(ConHero->ddP, 0);
                for (u32 E = 0;
                    E < 3;
                    ++E)
                {
#if 1
                    if (NoPush || (TimerIsUp && (Square(ddP2.E[E]) < 0.1f)))
#else
                    if (NoPush)
#endif
                    {
                        ddP2.E[E] = Cp*(ClosestP.E[E] - Head->P.E[E]) - 30.0f*Head->dP.E[E];
                    }
                }
                Head->MoveSpec.UnitMaxAccelVector = true;
                Head->MoveSpec.Speed = 30.0f;
                Head->MoveSpec.Drag = 8.0f;
                Head->ddP = ddP2;
            }

            if (Body)
            {
                Body->FacingDirection = Head->FacingDirection;
                Body->dP = V3(0, 0, 0);
        
                if (Body->MovementMode == MovementMode_Planted)
                {
                    if (Head)
                    {
                        r32 HeadDistance = 0.0f;
                        HeadDistance = Length(Head->P - Body->P);

                        r32 MaxHeadDistance = 0.5f;
                        r32 tHeadDistance = Clamp01MapToRange(0.0f, HeadDistance, MaxHeadDistance);
                        Body->ddtBob = -20.0f*tHeadDistance;
                    }

                    Body->P = GetSimSpaceTraversable(Body->Occupying).P;
                }
                
                v3 HeadDelta = {};
                if (Head)
                {
                    HeadDelta = Head->P - Body->P;
                }
                Body->FloorDisplace = (0.1f*HeadDelta).xy;
                Body->YAxis = V2(0, 1) + 0.5f*HeadDelta.xy;
            }

            if (Exited)
            {
                Exited = false;
                DeleteEntity(SimRegion, Head);
                DeleteEntity(SimRegion, Body);
                ConHero->BrainID.Value = 0;
            }
        } break;

        case Type_brain_snake:
        {
        } break;

        case Type_brain_familiar:
        {
            brain_familiar *Parts = &Brain->Familiar;
            entity *Head = Parts->Head;

            entity *ClosestHero = 0;
            real32 ClosestHeroDSq = Square(10.0f); // NOTE: Ten meter maximum search
            if (Global_AI_Familiar_FollowsHero)
            {
            
            }

            if (ClosestHero && (ClosestHeroDSq > Square(3.0f)))
            {
                real32 Acceleration = 0.5f;
                real32 OneOverLength = Acceleration / SquareRoot(ClosestHeroDSq);
                Head->ddP = OneOverLength*(ClosestHero->P - Head->P);
            }

            Head->MoveSpec.UnitMaxAccelVector = true;
            Head->MoveSpec.Speed = 50.0f;
            Head->MoveSpec.Drag = 8.0f;
        } break;

        case Type_brain_floaty_thing_for_now:
        {
            // TODO: Think about what this stuff actually  should mean
            // or does mean, or will mean
            //Entity->P.z += 0.01f*Sin(Entity->tBob);
            //Entity->tBob += dt;
        } break;

        case Type_brain_monstar:
        {
        } break;

        InvalidDefaultCase;
    }
} 