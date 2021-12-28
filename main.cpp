#include <common\IDebugLog.h>
#include <skse64_common\skse_version.h>
#include <skse64\GameInput.h>
#include <skse64\GameRTTI.h>
#include <skse64\PluginAPI.h>
#include <ShlObj.h>
#include <skse64/GameMenus.h>
#include <array>
#include <unordered_map>
#include "ImmersiveImpact/INILibrary/SimpleIni.h"
#include "ImmersiveImpact/Utils.h"

IDebugLog	gLog;
const char* logPath = "\\My Games\\Skyrim Special Edition\\SKSE\\OneClickPowerAttack.log";
const char* pluginName = "OneClickPowerAttack";

PluginHandle	g_pluginHandle = kPluginHandle_Invalid;
SKSEMessagingInterface* g_message = nullptr;
PlayerCharacter* p;
PlayerControls* pc;
MenuManager* mm;
InputManager* im;
IMenu* console;
CSimpleIniA ini(true, false, false);
int paKey = 257;
int altKey = 34;
int forceKeyCombination = -1;
int forceLeftKey = 999;
int forceRightKey = 999;
int forceDualKey = 999;
bool keyComboPressed = true;
bool onlyDuringAttacks = false;
bool skysaCompatibility = false;
std::string rightHand = "player.pa ActionRightPowerAttack";
std::string leftHand = "player.pa ActionLeftPowerAttack";
std::string dualWield = "player.pa ActionDualPowerAttack";
std::string skysaCommandL = "player.playidle LeftHandPowerAttackForward";
std::string skysaCommandR = "player.playidle PowerAttackForward";
std::string skysaCommandLStanding = "player.playidle LeftHandPowerAttack";
std::string skysaCommandRStanding = "player.playidle PowerAttackStanding";
bool isAttacking = false;

/*RelocAddr<uintptr_t> AttackStopHandler_vtable(0x1671F30);
RelocAddr<uintptr_t> AttackWinStartHandler_vtable(0x1671F00);
RelocAddr<uintptr_t> AttackWinEndHandler_vtable(0x1671F18);
RelocAddr<uintptr_t> WeaponRightSwingHandler_vable(0x01671ED0);
RelocAddr<uintptr_t> WeaponLeftSwingHandler_vtable(0x01671EE8);*/
RelocAddr<uintptr_t> PlayerCharacterAnimGraphEvent_vtable(0x1663F78);

bool IsRidingHorse(Actor* a) {
	return (a->actorState.flags04 & (3 << 14));
}

bool IsInKillmove(Actor* a) {
	return (a->flags2 & 0x00004000) == 0x00004000;
}

class HookAttackBlockHandler {
public:
	typedef void (HookAttackBlockHandler::* FnProcessButton) (ButtonEvent*, void*);
	void ProcessButton(ButtonEvent* a_event, void* a_data) {
		if (onlyDuringAttacks && isAttacking) {
			UInt32	keyCode;
			UInt32	deviceType = a_event->deviceType;
			UInt32	keyMask = a_event->keyMask;

			// Mouse
			if (deviceType == kDeviceType_Mouse)
				keyCode = InputMap::kMacro_MouseButtonOffset + keyMask;
			// Gamepad
			else if (deviceType == kDeviceType_Gamepad)
				keyCode = InputMap::GamepadMaskToKeycode(keyMask);
			// Keyboard
			else
				keyCode = keyMask;

			float timer = a_event->timer;
			bool isDown = a_event->flags != 0 && timer == 0.0;

			if (isDown && keyCode == paKey) {
				return;
			}
		}
		FnProcessButton fn = fnHash.at(*(UInt64*)this);
		if (fn)
			(this->*fn)(a_event, a_data);
	}

	static void Hook(uintptr_t ptr) {
		FnProcessButton fn = Utils::SafeWrite64Alt(*(uintptr_t*)ptr + 0x20, &HookAttackBlockHandler::ProcessButton);
		fnHash.insert(std::pair<UInt64, FnProcessButton>(*(uintptr_t*)ptr, fn));
	}
private:
	static std::unordered_map<UInt64, FnProcessButton> fnHash;
};
std::unordered_map<UInt64, HookAttackBlockHandler::FnProcessButton> HookAttackBlockHandler::fnHash;

/*class HookWeaponSwingHandler {
public:
	typedef UInt8 (HookWeaponSwingHandler::*FnProcessAction) (Actor*, UInt32);
	UInt8 ProcessAction(Actor* actor, UInt32 unk04) {
		if (onlyDuringAttacks) {
			int meleeState = (actor->actorState.flags04 >> 28 & 31);
			_MESSAGE("WeaponSwing meleeState %d", meleeState);
			if (actor == p && meleeState >= 1 && meleeState <= 6) {
				isAttacking = true;
			}
		}
		FnProcessAction fn = fnHash.at(*(UInt64*)this);
		return fn ? (this->*fn)(actor, unk04) : 0;
	}

	static void Hook() {
		FnProcessAction fn = Utils::SafeWrite64Alt(WeaponRightSwingHandler_vable.GetUIntPtr() + 0x8, &HookWeaponSwingHandler::ProcessAction);
		fnHash.insert(std::pair<UInt64, FnProcessAction>(WeaponRightSwingHandler_vable.GetUIntPtr(), fn));
		fn = Utils::SafeWrite64Alt(WeaponLeftSwingHandler_vtable.GetUIntPtr() + 0x8, &HookWeaponSwingHandler::ProcessAction);
		fnHash.insert(std::pair<UInt64, FnProcessAction>(WeaponLeftSwingHandler_vtable.GetUIntPtr(), fn));
	}
private:
	static std::unordered_map<UInt64, FnProcessAction> fnHash;
};
std::unordered_map<UInt64, HookWeaponSwingHandler::FnProcessAction> HookWeaponSwingHandler::fnHash;

class HookAttackStopHandler {
public:
	typedef UInt8 (HookAttackStopHandler::* FnProcessAction) (Actor*, UInt32);
	UInt8 ProcessAction(Actor* actor, UInt32 unk04) {
		if (onlyDuringAttacks) {
			int meleeState = (actor->actorState.flags04 >> 28 & 31);
			_MESSAGE("AttackStop meleeState %d", meleeState);
			if (actor == p && meleeState < 1 || meleeState > 6) {
				isAttacking = false;
			}
		}
		FnProcessAction fn = fnHash.at(*(UInt64*)this);
		return fn ? (this->*fn)(actor, unk04) : 0;
	}

	static void Hook() {
		FnProcessAction fn = Utils::SafeWrite64Alt(AttackStopHandler_vtable.GetUIntPtr() + 0x8, &HookAttackStopHandler::ProcessAction);
		fnHash.insert(std::pair<UInt64, FnProcessAction>(AttackStopHandler_vtable.GetUIntPtr(), fn));
	}
private:
	static std::unordered_map<UInt64, FnProcessAction> fnHash;
};
std::unordered_map<UInt64, HookAttackStopHandler::FnProcessAction> HookAttackStopHandler::fnHash;*/
class BSAnimationGraphEvent {
public:
	const BSFixedString eventname;
};

class HookAnimGraphEvent {
public:
	typedef EventResult (HookAnimGraphEvent::* FnReceiveEvent)(BSAnimationGraphEvent* evn, EventDispatcher<BSAnimationGraphEvent>* dispatcher);

	EventResult ReceiveEventHook(BSAnimationGraphEvent* evn, EventDispatcher<BSAnimationGraphEvent>* src) {
		Actor* a = *(Actor**)((uintptr_t)evn + 0x8);
		if (a) {
			if (!IsRidingHorse(a) && !IsInKillmove(a)) {
				int meleeState = (a->actorState.flags04 >> 28 & 31);
				if (meleeState >= 1 && meleeState <= 5) {
					isAttacking = true;
				}
				else if (meleeState < 1 || meleeState > 5) {
					isAttacking = false;
				}
			}
			else {
				isAttacking = false;
			}
		}
		FnReceiveEvent fn = fnHash.at(*(UInt64*)this);
		return fn ? (this->*fn)(evn, src) : kEvent_Continue;
	}

	static void Hook() {
		FnReceiveEvent fn = Utils::SafeWrite64Alt(PlayerCharacterAnimGraphEvent_vtable.GetUIntPtr() + 0x8, &HookAnimGraphEvent::ReceiveEventHook);
		fnHash.insert(std::pair<UInt64, FnReceiveEvent>(PlayerCharacterAnimGraphEvent_vtable.GetUIntPtr(), fn));
	}
private:
	static std::unordered_map<UInt64, FnReceiveEvent> fnHash;
};
std::unordered_map<UInt64, HookAnimGraphEvent::FnReceiveEvent> HookAnimGraphEvent::fnHash;

void SendConsoleCommand(std::string s) {
	GFxValue res;

	std::array<GFxValue, 2> args;
	args[0].SetString("ExecuteCommand");
	GFxValue str;
	console->view->CreateArray(&args[1]);
	str.SetString(s.c_str());
	args[1].PushBack(&str);

	console->view->Invoke("gfx.io.GameDelegate.call", &res, args.data(), args.size());
}

void LeftHandPowerAttack() {
	if (!skysaCompatibility) {
		SendConsoleCommand(leftHand);
	}
	else {
		SendConsoleCommand(skysaCommandL);
		SendConsoleCommand(skysaCommandLStanding);
	}
}

void DualWieldPowerAttack() {
	if (!skysaCompatibility) {
		SendConsoleCommand(dualWield);
	}
	else {
		if (!isAttacking) {
			SendConsoleCommand(dualWield);
		}
		else {
			SendConsoleCommand(skysaCommandR);
			SendConsoleCommand(skysaCommandRStanding);
		}
	}
}

void RightHandPowerAttack() {
	if (!skysaCompatibility) {
		SendConsoleCommand(rightHand);
	}
	else {
		SendConsoleCommand(skysaCommandR);
		SendConsoleCommand(skysaCommandRStanding);
	}
}

enum USER_EVENT_FLAG {
	kNone = 0,
	kMovement = 1 << 0,
	kLooking = 1 << 1,
	kActivate = 1 << 2,
	kMenu = 1 << 3,
	kConsole = 1 << 4,
	kPOVSwitch = 1 << 5,
	kFighting = 1 << 6,
	kSneaking = 1 << 7,
	kMainFour = 1 << 8,
	kWheelZoom = 1 << 9,
	kJumping = 1 << 10,
	kVATS = 1 << 11,
	kInvalid = 1 << 31,
};

enum class SIT_SLEEP_STATE : std::uint32_t {
	kNormal = 0,
	kWantToSit = 1,
	kWaitingForSitAnim = 2,

	kIsSitting = 3,
	kRidingMount = static_cast<std::underlying_type_t<SIT_SLEEP_STATE>>(kIsSitting),

	kWantToStand = 4,

	kWantToSleep = 5,
	kWaitingForSleepAnim = 6,
	kIsSleeping = 7,
	kWantToWake = 8
};

struct ActorState1 {
public:
	// members
	std::uint32_t     movingBack : 1;        // 0:00
	std::uint32_t     movingForward : 1;     // 0:01
	std::uint32_t     movingRight : 1;       // 0:02
	std::uint32_t     movingLeft : 1;        // 0:03
	std::uint32_t     unk04 : 2;             // 0:04
	std::uint32_t     walking : 1;           // 0:06
	std::uint32_t     running : 1;           // 0:07
	std::uint32_t     sprinting : 1;         // 0:08
	std::uint32_t     sneaking : 1;          // 0:09
	std::uint32_t     swimming : 1;          // 0:10
	std::uint32_t     unk11 : 3;             // 0:11
	SIT_SLEEP_STATE   sitSleepState : 4;     // 0:14
	std::uint32_t         flyState : 3;          // 0:18
	std::uint32_t  lifeState : 4;         // 0:21
	std::uint32_t  knockState : 3;        // 0:25
	std::uint32_t meleeAttackState : 4;  // 0:28
};

BSFixedString dialogueMenu = BSFixedString("Dialogue Menu");
class InputEventHandler : public BSTEventSink <InputEvent> {
public:
	virtual EventResult ReceiveEvent(InputEvent** evns, InputEventDispatcher* dispatcher) {
		if (!*evns)
			return kEvent_Continue;

		if (!p->processManager || !p->processManager->middleProcess)
			return kEvent_Continue;

		if (IsRidingHorse(p) || IsInKillmove(p))
			return kEvent_Continue;

		if (mm->numPauseGame > 0 || (im->unk118 & USER_EVENT_FLAG::kMovement) == 0 || (im->unk118 & USER_EVENT_FLAG::kLooking) == 0 || mm->IsMenuOpen(&dialogueMenu)
			|| ((ActorState1*)((uintptr_t)p + 0xC0))->sitSleepState != SIT_SLEEP_STATE::kNormal || p->actorValueOwner.GetCurrent(26) <= 0) {
			return kEvent_Continue;
		}

		UInt64 wepL = *(UInt64*)((uintptr_t)p->processManager->middleProcess + 0x220);
		UInt64 wepR = *(UInt64*)((uintptr_t)p->processManager->middleProcess + 0x260);
		UInt64 wepLR = *(UInt64*)((uintptr_t)p->processManager->middleProcess + 0x268);

		if (wepLR && ((*(TESObjectWEAP**)wepLR)->type() == TESObjectWEAP::GameData::kType_Bow || (*(TESObjectWEAP**)wepLR)->type() == TESObjectWEAP::GameData::kType_CrossBow || (*(TESObjectWEAP**)wepLR)->type() == TESObjectWEAP::GameData::kType_Staff)) {
			return kEvent_Continue;
		}
		SpellItem* magicL = p->leftHandSpell;
		SpellItem* magicR = p->rightHandSpell;
		for (InputEvent* e = *evns; e; e = e->next) {
			switch (e->eventType) {
				case InputEvent::kEventType_Button:
				{
					ButtonEvent* t = DYNAMIC_CAST(e, InputEvent, ButtonEvent);


					UInt32	keyCode;
					UInt32	deviceType = t->deviceType;
					UInt32	keyMask = t->keyMask;

					// Mouse
					if (deviceType == kDeviceType_Mouse)
						keyCode = InputMap::kMacro_MouseButtonOffset + keyMask;
					// Gamepad
					else if (deviceType == kDeviceType_Gamepad)
						keyCode = InputMap::GamepadMaskToKeycode(keyMask);
					// Keyboard
					else
						keyCode = keyMask;

					// Valid scancode?
					if (keyCode >= InputMap::kMaxMacros)
						continue;

					float timer = t->timer;
					bool isDown = t->flags != 0 && timer == 0.0;
					bool isUp = t->flags == 0 && timer != 0;

					if (isDown) {
						if (forceKeyCombination != -1 && keyCode == forceKeyCombination) {
							keyComboPressed = true;
						}
						if (console && console->view) {
							if (keyCode == paKey || keyCode == altKey) {
								bool succ = false;
								//Case 1 : Left weapon equipped, none on the right
								//Case 2 : Dual wield + Fist(nothing held, no magic)
								//Case 3 : Right hand equipped + possibly shield, Two handed
								if (wepL && !wepR && (!magicR || (onlyDuringAttacks && isAttacking))) {
									LeftHandPowerAttack();
								}
								else if (((wepL && wepR) || (!wepL && !wepR && !magicL && !magicR)) && (!onlyDuringAttacks || (onlyDuringAttacks && isAttacking))) {
									DualWieldPowerAttack();
								}
								else if ((!onlyDuringAttacks && !magicL && !magicR) || (onlyDuringAttacks && isAttacking)) {
									RightHandPowerAttack();
								}
							}
							if (keyComboPressed) {
								if (keyCode == forceLeftKey) {
									LeftHandPowerAttack();
								}
								if (keyCode == forceDualKey) {
									DualWieldPowerAttack();
								}
								if (keyCode == forceRightKey) {
									RightHandPowerAttack();
								}
							}
						}
					}
					else if (isUp) {
						if (forceKeyCombination != -1 && keyCode == forceKeyCombination) {
							keyComboPressed = false;
						}
					}
				}
				break;
			}
		}

		return kEvent_Continue;
	}
};

void LoadConfigs() {
	_MESSAGE("Loading configs");
	ini.LoadFile("Data\\SKSE\\Plugins\\OneClickPowerAttack.ini");
	paKey = std::stoi(ini.GetValue("General", "Keycode", "257"));
	altKey = std::stoi(ini.GetValue("General", "AltKey", "34"));
	forceKeyCombination = std::stoi(ini.GetValue("General", "ForceKeyCombination", "-1"));
	forceRightKey = std::stoi(ini.GetValue("General", "ForceRightKey", "999"));
	forceDualKey = std::stoi(ini.GetValue("General", "ForceDualKey", "999"));
	forceLeftKey = std::stoi(ini.GetValue("General", "ForceLeftKey", "999"));
	onlyDuringAttacks = std::stoi(ini.GetValue("General", "OnlyDuringAttack", "0")) > 0;
	skysaCompatibility = std::stoi(ini.GetValue("General", "SkySACompatibility", "0")) > 0;
	rightHand = ini.GetValue("General", "Right", "player.pa ActionRightPowerAttack");
	leftHand = ini.GetValue("General", "Left", "player.pa ActionLeftPowerAttack");
	dualWield = ini.GetValue("General", "Dual", "player.pa ActionDualPowerAttack");
	ini.Reset();
	_MESSAGE("Keycode %d", paKey);
	_MESSAGE("Done");

	isAttacking = false;
	if (forceKeyCombination != -1) {
		keyComboPressed = false;
	}
	else {
		keyComboPressed = true;
	}
}

class MenuWatcher : public BSTEventSink<MenuOpenCloseEvent> {
	virtual EventResult	ReceiveEvent(MenuOpenCloseEvent* evn, EventDispatcher<MenuOpenCloseEvent>* dispatcher) override {
		if (!console) {
			console = MenuManager::GetSingleton()->GetMenu(&BSFixedString("Console"));
			if(console)
				_MESSAGE("Console %llx", console);
			//MenuManager::GetSingleton()->MenuOpenCloseEventDispatcher()->RemoveEventSink(this);
		}
		if (evn->menuName == UIStringHolder::GetSingleton()->loadingMenu && evn->opening) {
			LoadConfigs();
		}
		return kEvent_Continue;
	}
};

extern "C" {
	bool SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info) {
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, logPath);
		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = pluginName;
		info->version = 1;

		// store plugin handle so we can identify ourselves later
		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor) {
			_MESSAGE("loaded in editor, marking as incompatible");

			return false;
		}
		else if (skse->runtimeVersion != RUNTIME_VERSION_1_5_97) {
			_MESSAGE("You're running this mod on runtime version %08X. This mod is designed for 1.5.97 so it may not work perfectly.", skse->runtimeVersion);
		}
		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface* skse) {
		g_message = (SKSEMessagingInterface*)skse->QueryInterface(kInterface_Messaging);
		g_message->RegisterListener(skse->GetPluginHandle(), "SKSE", [](SKSEMessagingInterface::Message* msg) -> void {
			if (msg->type == SKSEMessagingInterface::kMessage_DataLoaded) {
				InputEventDispatcher* inputEventDispatcher = InputEventDispatcher::GetSingleton();
				if (inputEventDispatcher) {
					p = *g_thePlayer;
					pc = PlayerControls::GetSingleton();
					im = InputManager::GetSingleton();
					mm = MenuManager::GetSingleton();
					InputEventHandler* handler = new InputEventHandler();
					inputEventDispatcher->AddEventSink(handler);
					MenuWatcher* mw = new MenuWatcher();;
					mm->MenuOpenCloseEventDispatcher()->AddEventSink(mw);
					UIManager* ui = UIManager::GetSingleton();
					UIStringHolder* uistr = UIStringHolder::GetSingleton();
					CALL_MEMBER_FN(ui, AddMessage)(&uistr->console, UIMessage::kMessage_Open, nullptr);
					CALL_MEMBER_FN(ui, AddMessage)(&uistr->console, UIMessage::kMessage_Close, nullptr);
					/*HookWeaponSwingHandler::Hook();
					HookAttackStopHandler::Hook();*/
					HookAnimGraphEvent::Hook();
					HookAttackBlockHandler::Hook((uintptr_t)pc->attackBlockHandler);
					_MESSAGE("PlayerCharacter %llx", p);
				}
				else {
					_MESSAGE("Failed to register inputEventHandler");
				}
			}
		});
		return true;
	}
};