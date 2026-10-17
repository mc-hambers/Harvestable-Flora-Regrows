
#include <SKSE/Impl/PCH.h>
#include <SKSE/SKSE.h>
#include <RE/T/TES.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/C/CrosshairPickData.h>
#include <RE/F/FormTraits.h>
#include <RE/T/TESFlora.h>
#include <RE/T/TESObjectTREE.h>
#include <RE/T/TESObjectLoadedEvent.h>
#include <RE/S/ScriptEventSourceHolder.h>
#include <RE/C/Calendar.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/V/VirtualMachine.h>
#include <RE/F/FunctionArguments.h>
#include <RE/N/NativeFunction.h>
#include <RE/U/UI.h>
#include <Windows.h>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <random>
#include <thread>
#include <atomic>
#include <chrono>
#include "TimerRules.h"
#include "FeatureRules.h"
#include <RE/T/TESDataHandler.h>
#include <RE/T/TESRace.h>
#include <RE/A/Actor.h>
#include <RE/B/BGSListForm.h>
#include <RE/O/Object.h>
#include <RE/I/IObjectHandlePolicy.h>
#include <vector>
#include <array>
using namespace std::literals;
namespace HFR {
 constexpr auto config=L"Data/SKSE/Plugins/HarvestableFloraRegrows.ini";
 std::mutex mutex;
 int minimum=3,maximum=5;
 std::array<int,7> options{1,1,0,4,0,0,4};
 constexpr const wchar_t* optionNames[]={L"ImmersiveInteractions",L"NirnrootEnabled",L"NirnrootCustomTime",L"NirnrootDays",L"CrimsonEnabled",L"CrimsonCustomTime",L"CrimsonDays"};
 RE::BGSListForm* usedList=nullptr;
 RE::TESRace* cowRace=nullptr;
 bool milkInitialized=false;
 std::unordered_map<RE::FormID,double> milkDeadlines;
 std::unordered_set<RE::FormID> legacyMilk;
 std::size_t milkCursor=0;
 constexpr std::uint32_t milkRecord=0x4D494C4B,metaRecord=0x4D455441,legacyRecord=0x4C454743;
 std::unordered_map<RE::FormID,double> deadlines;
 std::unordered_set<RE::FormID> loaded;
 std::deque<RE::FormID> roundRobin;
 std::mt19937 random{std::random_device{}()};
 std::atomic<bool> ready{false};
 std::atomic<bool> tickQueued{false};
 std::jthread scheduler;
 std::chrono::steady_clock::time_point lastScan{},lastLog{};
 constexpr std::uint32_t recordType=0x54494D52;
 double Now(){auto* c=RE::Calendar::GetSingleton();return c ? c->GetCurrentGameTime() : 0;}
 int Kind(RE::TESObjectREFR* ref){
  auto* base=ref ? ref->GetBaseObject() : nullptr;
  if(!base)return 0;
  if(int kind=NirnKindForBase(base->GetFormID()))return kind;
  RE::TESBoundObject* produce=nullptr;
  if(auto* f=base->As<RE::TESFlora>())produce=f->produceItem;
  if(auto* t=base->As<RE::TESObjectTREE>())produce=t->produceItem;
  return produce ? (produce->GetFormID()==0x59B86 ? 1 : produce->GetFormID()==0xB701A ? 2 : 0) : 0;
 }
 bool ScriptedNirn(RE::TESObjectREFR* ref){return ref && ref->GetBaseObject() && IsNirnBase(ref->GetBaseObject()->GetFormID());}
 bool Enabled(RE::TESObjectREFR* ref){int kind=Kind(ref);return kind==0 || options[kind==1 ? 1 : 4]!=0;}
 bool Eligible(RE::TESObjectREFR* ref){
  if(!ref || ref->IsDeleted())return false;
  if(ScriptedNirn(ref))return true;
  if(ref->IsDisabled())return false;
  auto* base=ref->GetBaseObject();if(!base)return false;
  if(auto* flora=base->As<RE::TESFlora>())return flora->produceItem!=nullptr;
  if(auto* tree=base->As<RE::TESObjectTREE>())return tree->produceItem!=nullptr;
  return false;
 }
 bool Harvested(RE::TESObjectREFR* ref){
  if(!ScriptedNirn(ref))return (ref->GetFormFlags() & (1u<<13))!=0;
  auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();auto* policy=vm ? vm->GetObjectHandlePolicy() : nullptr;
  if(!policy)return false;
  RE::BSTSmartPointer<RE::BSScript::Object> object;
  auto handle=policy->GetHandleForObject(ref->GetFormType(),ref);
  return vm->FindBoundObject(handle,"NirnrootACTIVATORScript",object) && object && object->currentState=="AlreadyHarvested";
 }
 bool InLoadedCell(RE::TESObjectREFR* ref){auto* cell=ref ? ref->GetParentCell() : nullptr;return ref && (ref->Is3DLoaded() || (cell && cell->IsAttached()));}
 void Track(RE::FormID id){
  auto* ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
  if(!Eligible(ref) || !InLoadedCell(ref))return;
  std::lock_guard lock(mutex);
  if(loaded.insert(id).second)roundRobin.push_back(id);
 }
 int GetMinimum(RE::StaticFunctionTag*){std::lock_guard lock(mutex);return minimum;}
 int GetMaximum(RE::StaticFunctionTag*){std::lock_guard lock(mutex);return maximum;}
 void Persist(){WritePrivateProfileStringW(L"Regrowth",L"MinimumDays",std::to_wstring(minimum).c_str(),config);WritePrivateProfileStringW(L"Regrowth",L"MaximumDays",std::to_wstring(maximum).c_str(),config);}
 void SetMinimum(RE::StaticFunctionTag*,int value){std::lock_guard lock(mutex);minimum=BoundDays(value);maximum=std::max(minimum,maximum);Persist();}
 void SetMaximum(RE::StaticFunctionTag*,int value){std::lock_guard lock(mutex);maximum=std::max(minimum,BoundDays(value));Persist();}
 void Completed(RE::StaticFunctionTag*,RE::TESObjectREFR* ref){
  if(!ref)return;
  std::lock_guard lock(mutex);
  if(!Harvested(ref)){
   deadlines.erase(ref->GetFormID());
   SKSE::log::info("Regrew plant {:08X}",ref->GetFormID());
  }else SKSE::log::warn("Plant {:08X} still harvested after SetHarvested",ref->GetFormID());
 }
 void Inspect(RE::StaticFunctionTag*) {
  SKSE::GetTaskInterface()->AddTask([]{
   auto* pick=RE::CrosshairPickData::GetSingleton();auto ref=pick ? pick->GetActiveTarget().get() : RE::NiPointer<RE::TESObjectREFR>{};
   if(!ref){SKSE::log::info("Inspect: no crosshair reference; day {}",Now());}
   else {
    Track(ref->GetFormID());
    std::lock_guard lock(mutex);auto it=deadlines.find(ref->GetFormID());
    SKSE::log::info("Inspect: id={:08X}, eligible={}, loadedCell={}, 3D={}, harvested={}, flags={:08X}, day={}, deadline={}, remainingDays={}",ref->GetFormID(),Eligible(ref.get()),InLoadedCell(ref.get()),ref->Is3DLoaded(),Harvested(ref.get()),ref->GetFormFlags(),Now(),it==deadlines.end() ? -1 : it->second,it==deadlines.end() ? -1 : it->second-Now());
   }
   spdlog::default_logger()->flush();
  });
 }
 int GetOption(RE::StaticFunctionTag*,int index){std::lock_guard lock(mutex);return index>=0 && index<7 ? options[index] : 0;}
 void SetOption(RE::StaticFunctionTag*,int index,int value){
  if(index<0 || index>=7)return;
  std::lock_guard lock(mutex);options[index]=(index==3 || index==6) ? BoundDays(value) : (value!=0);
  WritePrivateProfileStringW(L"Features",optionNames[index],std::to_wstring(options[index]).c_str(),config);
 }
 bool MilkAvailable(RE::StaticFunctionTag*){return usedList && cowRace;}
 bool IsCow(RE::TESForm* form){auto* actor=form ? form->As<RE::Actor>() : nullptr;return actor && !actor->IsDead() && actor->GetRace()==cowRace;}
 void MilkCompleted(RE::StaticFunctionTag*,RE::TESObjectREFR* cow){
  if(!cow || !usedList || usedList->HasForm(cow))return;
  std::lock_guard lock(mutex);milkDeadlines.erase(cow->GetFormID());legacyMilk.erase(cow->GetFormID());
 }
 bool CanRestoreMilk(RE::StaticFunctionTag*,RE::TESObjectREFR* cow){
  std::lock_guard lock(mutex);return options[0] && usedList && IsCow(cow) && usedList->HasForm(cow);
 }
 bool CanRegrow(RE::StaticFunctionTag*,RE::TESObjectREFR* plant){
  std::lock_guard lock(mutex);return Eligible(plant) && InLoadedCell(plant) && Enabled(plant) && Harvested(plant);
 }
 bool IsScriptedNirn(RE::StaticFunctionTag*,RE::TESObjectREFR* plant){return ScriptedNirn(plant);}
 void TickMilk(double now){
  if(!usedList || !cowRace)return;
  {std::lock_guard lock(mutex);if(!options[0])return;}
  // Snapshot IDs before any VM call can mutate the shared formlist.
  std::vector<RE::FormID> snapshot;
  for(auto* form:usedList->forms)if(form)snapshot.push_back(form->GetFormID());
  if(usedList->scriptAddedTempForms)for(auto id:*usedList->scriptAddedTempForms)snapshot.push_back(id);
  std::unordered_set<RE::FormID> present(snapshot.begin(),snapshot.end());
  {std::lock_guard lock(mutex);
   if(!milkInitialized){legacyMilk=present;milkInitialized=true;SKSE::log::info("Milk adapter initializing: {} existing interaction entries",snapshot.size());}
   std::erase_if(milkDeadlines,[&](const auto& entry){return !present.contains(entry.first);});
   std::erase_if(legacyMilk,[&](auto id){return !present.contains(id);});
  }
  if(snapshot.empty())return;
  const auto count=std::min<std::size_t>(snapshot.size(),128);unsigned requests=0;
  for(std::size_t n=0;n<count;n++){
   auto id=snapshot[milkCursor++%snapshot.size()];auto* form=RE::TESForm::LookupByID(id);
   if(!form)continue; // Unresolved legacy entries stay pending without forcing their cell to load.
   if(!IsCow(form)){std::lock_guard lock(mutex);legacyMilk.erase(id);milkDeadlines.erase(id);continue;}
   bool due=false;
   {std::lock_guard lock(mutex);
    if(legacyMilk.contains(id))due=true;
    else {
     auto it=milkDeadlines.find(id);
     if(it==milkDeadlines.end())milkDeadlines.emplace(id,Deadline(now,minimum,maximum,std::generate_canonical<double,53>(random)));
     else due=MilkDue(false,now,it->second);
    }
   }
   if(due && requests<16){
    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();if(!vm)continue;
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb;
    vm->DispatchStaticCall("HFR_Worker","RestoreMilk",RE::MakeFunctionArguments(static_cast<RE::BGSListForm*>(usedList),static_cast<RE::TESObjectREFR*>(form->As<RE::TESObjectREFR>())),cb);
    requests++;
   }
  }
 }
 bool Register(RE::BSScript::IVirtualMachine* vm){
  vm->RegisterFunction("GetOption","HFR_Native",GetOption);vm->RegisterFunction("SetOption","HFR_Native",SetOption);
  vm->RegisterFunction("MilkAvailable","HFR_Native",MilkAvailable);vm->RegisterFunction("MilkCompleted","HFR_Native",MilkCompleted);
  vm->RegisterFunction("CanRestoreMilk","HFR_Native",CanRestoreMilk);vm->RegisterFunction("CanRegrow","HFR_Native",CanRegrow);vm->RegisterFunction("IsScriptedNirn","HFR_Native",IsScriptedNirn);
  vm->RegisterFunction("Inspect","HFR_Native",Inspect);
  vm->RegisterFunction("Completed","HFR_Native",Completed);
  vm->RegisterFunction("GetMinimum","HFR_Native",GetMinimum);vm->RegisterFunction("GetMaximum","HFR_Native",GetMaximum);
  vm->RegisterFunction("SetMinimum","HFR_Native",SetMinimum);vm->RegisterFunction("SetMaximum","HFR_Native",SetMaximum);return true;
 }
 void RequestRegrow(RE::TESObjectREFR* ref){
  auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();if(!vm)return;
  auto* args=RE::MakeFunctionArguments(static_cast<RE::TESObjectREFR*>(ref));
  RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
  SKSE::log::info("Request regrowth {:08X} at game day {}",ref->GetFormID(),Now());
  if(!vm->DispatchStaticCall("HFR_Worker","Regrow",args,callback))SKSE::log::warn("Regrowth dispatch rejected for {:08X}",ref->GetFormID());
 }
 void Tick(){
  if(!ready)return;
  auto* ui=RE::UI::GetSingleton();if(!ui || ui->GameIsPaused())return;
  const auto realNow=std::chrono::steady_clock::now();
  if(realNow-lastScan>=std::chrono::seconds(10)){
   lastScan=realNow;
   if(auto* world=RE::TES::GetSingleton())world->ForEachReference([](RE::TESObjectREFR* ref){if(ref)Track(ref->GetFormID());return RE::BSContainer::ForEachResult::kContinue;});
  }
  const double now=Now();if(!std::isfinite(now))return;
  TickMilk(now);
  std::size_t count;{std::lock_guard lock(mutex);count=std::min<std::size_t>(roundRobin.size(),256);}
  unsigned restores=0;
  for(std::size_t i=0;i<count;i++){
   RE::FormID id;
   {std::lock_guard lock(mutex);id=roundRobin.front();roundRobin.pop_front();if(!loaded.contains(id))continue;}
   auto* ref=RE::TESForm::LookupByID<RE::TESObjectREFR>(id);
   if(!Eligible(ref) || !InLoadedCell(ref)){std::lock_guard lock(mutex);loaded.erase(id);continue;}
   {std::lock_guard lock(mutex);roundRobin.push_back(id);}
   bool due=false;
   {std::lock_guard lock(mutex);
    if(!Enabled(ref))continue;
    if(!Harvested(ref)){deadlines.erase(id);continue;}
    auto it=deadlines.find(id);
    if(it==deadlines.end()){
     const double roll=std::generate_canonical<double,53>(random);
     const int kind=Kind(ref);
     const int offset=kind==1 ? 1 : 4;
     const double dueAt=FeatureDeadline(now,minimum,maximum,kind!=0 && options[offset+1]!=0,kind!=0 ? options[offset+2] : minimum,roll);
     deadlines.emplace(id,dueAt);
     SKSE::log::info("Timer assigned {:08X}: day {} -> {}",id,now,dueAt);
    }else due=now>=it->second;
   }
   // Keep the deadline until the game confirms the harvested flag was cleared.
   if(due && restores<16){RequestRegrow(ref);restores++;}
  }
  if(realNow-lastLog>=std::chrono::seconds(30)){
   lastLog=realNow;std::lock_guard lock(mutex);
   SKSE::log::info("Tracking heartbeat: day={}, loadedPlants={}, timers={}, checked={}, requests={}",now,loaded.size(),deadlines.size(),count,restores);
   spdlog::default_logger()->flush();
  }
 }
 class Events final:public RE::BSTEventSink<RE::TESObjectLoadedEvent>{
  RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* e,RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override{
   if(e){const auto id=e->formID;if(e->loaded)SKSE::GetTaskInterface()->AddTask([id]{Track(id);});// Unloaded entries are removed when their queue slot is visited, avoiding duplicate queue entries on rapid reload.
   }
   return RE::BSEventNotifyControl::kContinue;
  }
 } events;
 void BeginScheduler(){
  scheduler=std::jthread([](std::stop_token stop){
   while(!stop.stop_requested()){
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    if(!ready || tickQueued.exchange(true))continue;
    SKSE::GetTaskInterface()->AddTask([]{tickQueued=false;Tick();});
    // Time checks and all reference access run only on the game main thread.
    for(int i=0;i<3 && !stop.stop_requested();i++)std::this_thread::sleep_for(std::chrono::milliseconds(500));
   }
  });
 }
 void Reset(SKSE::SerializationInterface*){std::lock_guard lock(mutex);ready=false;deadlines.clear();loaded.clear();roundRobin.clear();milkDeadlines.clear();legacyMilk.clear();milkInitialized=false;milkCursor=0;}
 void Save(SKSE::SerializationInterface* s){
  std::lock_guard lock(mutex);
  auto writeTimers=[&](auto type,const auto& timers){if(s->OpenRecord(type,1))for(const auto& [id,due]:timers){if(!s->WriteRecordData(id)||!s->WriteRecordData(due))break;}};
  writeTimers(recordType,deadlines);writeTimers(milkRecord,milkDeadlines);
  std::uint32_t initialized=milkInitialized ? 1 : 0;s->WriteRecord(metaRecord,1,initialized);
  if(s->OpenRecord(legacyRecord,1))for(auto id:legacyMilk)if(!s->WriteRecordData(id))break;
 }
 void Load(SKSE::SerializationInterface* s){
  Reset(s);std::uint32_t type,version,length;
  while(s->GetNextRecordInfo(type,version,length)){
   if(version!=1 || length>12000000)continue;
   if(type==metaRecord && length==4){std::uint32_t value=0;if(s->ReadRecordData(value)==4)milkInitialized=value!=0;continue;}
   if(type==legacyRecord && length%4==0){for(std::uint32_t n=0;n<length/4;n++){RE::FormID oldID,newID;if(s->ReadRecordData(oldID)!=4)break;if(s->ResolveFormID(oldID,newID))legacyMilk.insert(newID);}continue;}
   if((type!=recordType && type!=milkRecord) || length%12)continue;
   for(std::uint32_t n=0;n<length/12;n++){
    RE::FormID oldID,newID;double due;
    if(s->ReadRecordData(oldID)!=4 || s->ReadRecordData(due)!=8)break;
    if(std::isfinite(due) && due>=0 && s->ResolveFormID(oldID,newID)){std::lock_guard lock(mutex);(type==recordType ? deadlines : milkDeadlines)[newID]=due;}
   }
  }
 }
 void Start(){
  SKSE::GetTaskInterface()->AddTask([]{
   {std::lock_guard lock(mutex);loaded.clear();roundRobin.clear();}
   if(auto* world=RE::TES::GetSingleton())world->ForEachReference([](RE::TESObjectREFR* ref){if(ref)Track(ref->GetFormID());return RE::BSContainer::ForEachResult::kContinue;});
   TickMilk(Now());
   ready=true;lastScan={};lastLog={};SKSE::log::info("Regrowth tracking started; {} persisted plants, {} loaded plants",deadlines.size(),loaded.size());
   spdlog::default_logger()->flush();
  });
 }
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[]{
 SKSE::PluginVersionData d;d.PluginVersion({1,1,1,0});d.PluginName("HarvestableFloraRegrows");d.AuthorName("Max");d.UsesAddressLibrary();d.UsesNoStructs();return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface* s,SKSE::PluginInfo* i){i->infoVersion=SKSE::PluginInfo::kVersion;i->name="HarvestableFloraRegrows";i->version=REL::Version(1,1,1,0).pack();return !s->IsEditor() && s->RuntimeVersion()>=REL::Version(1,5,97,0);}
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse){
 SKSE::Init(skse);
 static HANDLE instance;auto name=L"Local\\HarvestableFloraRegrows."+std::to_wstring(GetCurrentProcessId());instance=CreateMutexW(nullptr,FALSE,name.c_str());
 if(instance && GetLastError()==ERROR_ALREADY_EXISTS)SKSE::stl::report_and_fail("Multiple Harvestable Flora Regrows DLLs loaded. Remove duplicate versions.");
 for(int n=0;n<7;n++)HFR::options[n]=(n==3 || n==6) ? BoundDays(GetPrivateProfileIntW(L"Features",HFR::optionNames[n],4,HFR::config)) : GetPrivateProfileIntW(L"Features",HFR::optionNames[n],HFR::options[n],HFR::config)!=0;
 HFR::minimum=BoundDays(GetPrivateProfileIntW(L"Regrowth",L"MinimumDays",3,HFR::config));HFR::maximum=std::max(HFR::minimum,BoundDays(GetPrivateProfileIntW(L"Regrowth",L"MaximumDays",5,HFR::config)));
 auto* serial=SKSE::GetSerializationInterface();serial->SetUniqueID(0x48465247);serial->SetSaveCallback(HFR::Save);serial->SetLoadCallback(HFR::Load);serial->SetRevertCallback(HFR::Reset);
 if(!SKSE::GetPapyrusInterface()->Register(HFR::Register))return false;
 return SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* m){
  if(m->type==SKSE::MessagingInterface::kDataLoaded){auto* data=RE::TESDataHandler::GetSingleton();
   HFR::usedList=data->LookupForm<RE::BGSListForm>(0x80A,"ImmersiveInteractions.esp");
   HFR::cowRace=RE::TESForm::LookupByID<RE::TESRace>(0x4E785);
   SKSE::log::info("Immersive Interactions milk adapter: {}",HFR::MilkAvailable(nullptr) ? "available" : "not installed");
   RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&HFR::events);HFR::BeginScheduler();}
  if(m->type==SKSE::MessagingInterface::kPostLoadGame || m->type==SKSE::MessagingInterface::kNewGame)HFR::Start();
 });
}



