/*
 * Hardcoded names from Republic Commando
 */

/*-----------------------------------------------------------------------------
	Macros.
-----------------------------------------------------------------------------*/

// Define a message as an enumeration.
#ifndef REGISTER_NAME
	#define REGISTER_NAME(name) NAME_ ## name,
	#define REG_NAME_HIGH(name) NAME_ ## name,
	#define REGISTERING_ENUM
namespace uscript{
enum{
#endif

/*-----------------------------------------------------------------------------
	Hardcoded names which are not messages.
-----------------------------------------------------------------------------*/

// Special zero value, meaning no name
REG_NAME_HIGH(None)

// Class property types; these map straight onto hardcoded property types
REGISTER_NAME(ByteProperty)
REGISTER_NAME(IntProperty)
REGISTER_NAME(BoolProperty)
REGISTER_NAME(FloatProperty)
REGISTER_NAME(ObjectProperty)
REGISTER_NAME(NameProperty)
REGISTER_NAME(DelegateProperty)
REGISTER_NAME(ClassProperty)
REGISTER_NAME(ArrayProperty)
REGISTER_NAME(StructProperty)
REGISTER_NAME(VectorProperty)
REGISTER_NAME(RotatorProperty)
REGISTER_NAME(StrProperty)
REGISTER_NAME(MapProperty)
REGISTER_NAME(FixedArrayProperty)

// Packages
REGISTER_NAME(Core)
REGISTER_NAME(Engine)
REGISTER_NAME(Editor)
REGISTER_NAME(Gameplay)

// UnrealScript types
REG_NAME_HIGH(byte)
REG_NAME_HIGH(int)
REG_NAME_HIGH(bool)
REG_NAME_HIGH(float)
REG_NAME_HIGH(name)
REG_NAME_HIGH(string)
REG_NAME_HIGH(struct)
REG_NAME_HIGH(vector)
REG_NAME_HIGH(rotator)
REG_NAME_HIGH(color)
REG_NAME_HIGH(plane)
REG_NAME_HIGH(button)
REG_NAME_HIGH(CompressedPosition)

// Keywords
REGISTER_NAME(Begin)
REG_NAME_HIGH(State)
REG_NAME_HIGH(function)
REG_NAME_HIGH(self)
REG_NAME_HIGH(true)
REG_NAME_HIGH(false)
REG_NAME_HIGH(transient)
REG_NAME_HIGH(RuntimeStatic)
REG_NAME_HIGH(enum)
REG_NAME_HIGH(replication)
REG_NAME_HIGH(reliable)
REG_NAME_HIGH(unreliable)
REG_NAME_HIGH(always)

// Object class names
REGISTER_NAME(Field)
REGISTER_NAME(Object)
REGISTER_NAME(TextBuffer)
REGISTER_NAME(Linker)
REGISTER_NAME(LinkerLoad)
REGISTER_NAME(LinkerSave)
REGISTER_NAME(Subsystem)
REGISTER_NAME(Factory)
REGISTER_NAME(TextBufferFactory)
REGISTER_NAME(Exporter)
REGISTER_NAME(StackNode)
REGISTER_NAME(Property)
REGISTER_NAME(Camera)
REGISTER_NAME(PlayerInput)

// Creation and destruction
REGISTER_NAME(Spawned)
REGISTER_NAME(Destroyed)

// Gaining/losing actors
REGISTER_NAME(GainedChild)
REGISTER_NAME(LostChild)
REGISTER_NAME(Probe4)
REGISTER_NAME(Probe5)

// Triggers
REGISTER_NAME(Trigger)
REGISTER_NAME(UnTrigger)

// Physics & world interaction
REGISTER_NAME(Timer)
REGISTER_NAME(HitWall)
REGISTER_NAME(Falling)
REGISTER_NAME(Landed)
REGISTER_NAME(PhysicsVolumeChange)
REGISTER_NAME(Touch)
REGISTER_NAME(UnTouch)
REGISTER_NAME(Bump)
REGISTER_NAME(BeginState)
REGISTER_NAME(EndState)
REGISTER_NAME(BaseChange)
REGISTER_NAME(Attach)
REGISTER_NAME(Detach)
REGISTER_NAME(ActorEntered)
REGISTER_NAME(ActorLeaving)
REGISTER_NAME(ZoneChange)
REGISTER_NAME(AnimEnd)
REGISTER_NAME(EndedRotation)
REGISTER_NAME(InterpolateEnd)
REGISTER_NAME(EncroachingOn)
REGISTER_NAME(EncroachedBy)
REGISTER_NAME(NotifyTurningInPlace)
REGISTER_NAME(HeadVolumeChange)
REGISTER_NAME(PostTouch)
REGISTER_NAME(PawnEnteredVolume)
REGISTER_NAME(MayFall)
REGISTER_NAME(CheckDirectionChange)
REGISTER_NAME(PawnLeavingVolume)

// Updates
REGISTER_NAME(Tick)
REGISTER_NAME(PlayerTick)
REGISTER_NAME(ModifyVelocity)

// AI
REGISTER_NAME(CheckMovementTransition)
REGISTER_NAME(SeePlayer)
REGISTER_NAME(HearNoise)
REGISTER_NAME(UpdateEyeHeight)
REGISTER_NAME(SeeMonster)
REGISTER_NAME(BotDesireability)
REGISTER_NAME(NotifyBump)
REGISTER_NAME(NotifyPhysicsVolumeChange)
REGISTER_NAME(AIHearSound)
REGISTER_NAME(NotifyHeadVolumeChange)
REGISTER_NAME(NotifyLanded)
REGISTER_NAME(NotifyHitWall)
REGISTER_NAME(PostNetReceive)
REGISTER_NAME(PreBeginPlay)
REGISTER_NAME(BeginPlay)
REGISTER_NAME(PostBeginPlay)
REGISTER_NAME(PostLoadBeginPlay)
REGISTER_NAME(PhysicsChangedFor)
REGISTER_NAME(ActorEnteredVolume)
REGISTER_NAME(ActorLeavingVolume)
REGISTER_NAME(PrepareForMove)

// Special tag meaning 'All probes'
REGISTER_NAME(All)

REGISTER_NAME(SceneStarted)
REGISTER_NAME(SceneEnded)

// Constants
REG_NAME_HIGH(vect)
REG_NAME_HIGH(rot)
REG_NAME_HIGH(arraycount)
REG_NAME_HIGH(enumcount)
REG_NAME_HIGH(rng)

// Flow control
REG_NAME_HIGH(else)
REG_NAME_HIGH(if)
REG_NAME_HIGH(goto)
REG_NAME_HIGH(stop)
REG_NAME_HIGH(until)
REG_NAME_HIGH(while)
REG_NAME_HIGH(do)
REG_NAME_HIGH(break)
REG_NAME_HIGH(for)
REG_NAME_HIGH(foreach)
REG_NAME_HIGH(assert)
REG_NAME_HIGH(switch)
REG_NAME_HIGH(case)
REG_NAME_HIGH(default)
REG_NAME_HIGH(continue)

// Variable overrides
REG_NAME_HIGH(private)
REG_NAME_HIGH(const)
REG_NAME_HIGH(out)
REG_NAME_HIGH(export)
REG_NAME_HIGH(edfindable)
REG_NAME_HIGH(skip)
REG_NAME_HIGH(coerce)
REG_NAME_HIGH(optional)
REG_NAME_HIGH(input)
REG_NAME_HIGH(config)
REG_NAME_HIGH(travel)
REG_NAME_HIGH(editconst)
REG_NAME_HIGH(localized)
REG_NAME_HIGH(globalconfig)
REG_NAME_HIGH(safereplace)
REG_NAME_HIGH(new)
REG_NAME_HIGH(protected)
REG_NAME_HIGH(public)
REG_NAME_HIGH(editinline)
REG_NAME_HIGH(editinlineuse)
REG_NAME_HIGH(deprecated)
REG_NAME_HIGH(editconstarray)
REG_NAME_HIGH(editinlinenotify)
REG_NAME_HIGH(automated)
REG_NAME_HIGH(pconly)
REG_NAME_HIGH(align)
REG_NAME_HIGH(autoload)

// Class overrides
REG_NAME_HIGH(intrinsic)
REG_NAME_HIGH(within)
REG_NAME_HIGH(abstract)
REG_NAME_HIGH(package)
REG_NAME_HIGH(guid)
REG_NAME_HIGH(parent)
REG_NAME_HIGH(class)
REG_NAME_HIGH(extends)
REG_NAME_HIGH(noexport)
REG_NAME_HIGH(placeable)
REG_NAME_HIGH(perobjectconfig)
REG_NAME_HIGH(nativereplication)
REG_NAME_HIGH(notplaceable)
REG_NAME_HIGH(editinlinenew)
REG_NAME_HIGH(noteditinlinenew)
REG_NAME_HIGH(hidecategories)
REG_NAME_HIGH(showcategories)
REG_NAME_HIGH(collapsecategories)
REG_NAME_HIGH(dontcollapsecategories)

// State overrides
REG_NAME_HIGH(auto)
REG_NAME_HIGH(ignores)

// Auto-instanced subobjects
REG_NAME_HIGH(instanced)

// Calling overrides
REG_NAME_HIGH(global)
REG_NAME_HIGH(super)
REG_NAME_HIGH(outer)

// Class overrides
REG_NAME_HIGH(dependson)

// Function Overrides
REG_NAME_HIGH(operator)
REG_NAME_HIGH(preoperator)
REG_NAME_HIGH(postoperator)
REG_NAME_HIGH(final)
REG_NAME_HIGH(iterator)
REG_NAME_HIGH(latent)
REG_NAME_HIGH(return)
REG_NAME_HIGH(singular)
REG_NAME_HIGH(simulated)
REG_NAME_HIGH(exec)
REG_NAME_HIGH(event)
REG_NAME_HIGH(static)
REG_NAME_HIGH(native)
REG_NAME_HIGH(invariant)
REG_NAME_HIGH(delegate)

// Variable overrides
REG_NAME_HIGH(var)
REG_NAME_HIGH(local)
REG_NAME_HIGH(import)
REG_NAME_HIGH(from)

// Special commands
REG_NAME_HIGH(Spawn)
REG_NAME_HIGH(array)
REG_NAME_HIGH(map)

// Misc
REGISTER_NAME(Tag)
REGISTER_NAME(Role)
REGISTER_NAME(RemoteRole)
REGISTER_NAME(System)
REGISTER_NAME(User)

// Log messages
REGISTER_NAME(Log)
REGISTER_NAME(Critical)
REGISTER_NAME(Init)
REGISTER_NAME(Exit)
REGISTER_NAME(Cmd)
REGISTER_NAME(Play)
REGISTER_NAME(Console)
REGISTER_NAME(Warning)
REGISTER_NAME(ExecWarning)
REGISTER_NAME(ScriptWarning)
REGISTER_NAME(ScriptLog)
REGISTER_NAME(Dev)
REGISTER_NAME(DevNet)
REGISTER_NAME(DevPath)
REGISTER_NAME(DevNetTraffic)
REGISTER_NAME(DevAudio)
REGISTER_NAME(DevLoad)
REGISTER_NAME(DevSave)
REGISTER_NAME(DevGarbage)
REGISTER_NAME(DevKill)
REGISTER_NAME(DevReplace)
REGISTER_NAME(DevMusic)
REGISTER_NAME(DevSound)
REGISTER_NAME(DevCompile)
REGISTER_NAME(DevBind)
REGISTER_NAME(Localization)
REGISTER_NAME(Compatibility)
REGISTER_NAME(NetComeGo)
REGISTER_NAME(Title)
REGISTER_NAME(Error)
REGISTER_NAME(Heading)
REGISTER_NAME(SubHeading)
REGISTER_NAME(FriendlyError)
REGISTER_NAME(Progress)
REGISTER_NAME(UserPrompt)
REGISTER_NAME(AILog)

/*-----------------------------------------------------------------------------
	Closing.
-----------------------------------------------------------------------------*/

#ifdef REGISTERING_ENUM
};
static_assert(NAME_None == 0);
}
	#undef REGISTER_NAME
	#undef REG_NAME_HIGH
	#undef REGISTERING_ENUM
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
