#include "ClientProtocol.h"
#include "ClientProcedures.h"
#include "ClientSocket.h"
#include "IPCProcedures.h"
#include "Notification.h"
#include "Server.h"

Void SendEventList(
    ServerContextRef Context,
    ClientContextRef Client
) {
    S2C_DATA_NFY_EVENT_LIST* EventList = PacketBufferInit(Client->Connection->PacketBuffer, S2C, NFY_EVENT_LIST);
    EventList->Count = Context->Runtime->Context->EventCount;

    for (Index Index = 0; Index < Context->Runtime->Context->EventCount; Index += 1) {
        RTDataEventRef EventData = &Context->Runtime->Context->EventList[Index];
        PacketBufferAppendValue(Client->Connection->PacketBuffer, UInt32, EventData->ID);
    }

    SocketSend(Context->ClientSocket, Client->Connection, EventList);
}

Void SendEventInfo(
    ServerContextRef Context,
    ClientContextRef Client
) {
    S2C_DATA_NFY_EVENT_INFO* EventInfo = PacketBufferInitExtended(Client->Connection->PacketBuffer, S2C, NFY_EVENT_INFO);
    EventInfo->HasEventInfo = (Context->Runtime->Context->EventCount > 0) ? 1 : 0;

    S2C_DATA_NFY_EVENT_INFO_HEADER* EventInfoHeader = PacketBufferAppendStruct(Client->Connection->PacketBuffer, S2C_DATA_NFY_EVENT_INFO_HEADER);
    EventInfoHeader->EventCount = Context->Runtime->Context->EventCount;

    for (Int32 EventIndex = 0; EventIndex < Context->Runtime->Context->EventCount; EventIndex += 1) {
        RTDataEventRef EventData = &Context->Runtime->Context->EventList[EventIndex];

        S2C_DATA_NFY_EVENT_INFO_EVENT* EventInfoEvent = PacketBufferAppendStruct(Client->Connection->PacketBuffer, S2C_DATA_NFY_EVENT_INFO_EVENT);
        EventInfoEvent->EventIndex = EventData->ID;
        EventInfoEvent->EventType = EventData->Type;
        EventInfoEvent->EventFlags = EventData->UseFlags;
        EventInfoEvent->EventStartTimestamp = GetTimestamp() - 1000000;
        EventInfoEvent->EventEndTimestamp = GetTimestamp() + 1000000;
        EventInfoEvent->EventUpdateTimestamp = 0;
        EventInfoEvent->WorldIndex = EventData->WorldIndex;
        EventInfoEvent->NpcIndex = EventData->NpcIndex;
        PacketBufferAppendCString(Client->Connection->PacketBuffer, EventData->Description);

        S2C_DATA_NFY_EVENT_INFO_EVENT_ITEM_INFO* EventItemInfo = PacketBufferAppendStruct(Client->Connection->PacketBuffer, S2C_DATA_NFY_EVENT_INFO_EVENT_ITEM_INFO);
        EventItemInfo->ItemCount = EventData->EventItemCount;

        for (Index ItemIndex = 0; ItemIndex < EventData->EventItemCount; ItemIndex += 1) {
            RTDataEventItemRef EventItem = &EventData->EventItemList[ItemIndex];

            S2C_DATA_NFY_EVENT_INFO_EVENT_ITEM_INFO_DATA* EventItemData = PacketBufferAppendStruct(Client->Connection->PacketBuffer, S2C_DATA_NFY_EVENT_INFO_EVENT_ITEM_INFO_DATA);
            EventItemData->ItemID.Serial = EventItem->ItemID;
            EventItemData->ItemOptions = EventItem->ItemOptions;
            EventItemData->ExternalID = EventItem->ExternalID;
            EventItemData->TextureItemID = EventItem->TextureItemID;
            PacketBufferAppendCString(Client->Connection->PacketBuffer, EventItem->Title);
            PacketBufferAppendCString(Client->Connection->PacketBuffer, EventItem->Description);
        }

        EventItemInfo = PacketBufferAppendStruct(Client->Connection->PacketBuffer, S2C_DATA_NFY_EVENT_INFO_EVENT_ITEM_INFO);
        EventItemInfo->ItemCount = 0;

        EventItemInfo = PacketBufferAppendStruct(Client->Connection->PacketBuffer, S2C_DATA_NFY_EVENT_INFO_EVENT_ITEM_INFO);
        EventItemInfo->ItemCount = 0;
    }

    SocketSend(Context->ClientSocket, Client->Connection, EventInfo);
}

CLIENT_PROCEDURE_BINDING(INITIALIZE) {
    if (Character) goto error;
	
    if (!(Client->Flags & CLIENT_FLAGS_CHARACTER_INDEX_LOADED) || Client->Account.AccountID < 1) goto error;

    IPC_W2D_DATA_GET_CHARACTER* Request = IPCPacketBufferInit(Server->IPCSocket->PacketBuffer, W2D, GET_CHARACTER);
    Request->Header.SourceConnectionID = Connection->ID;
    Request->Header.Source = Server->IPCSocket->NodeID;
    Request->Header.Target.Group = Context->Config.WorldSvr.GroupIndex;
    Request->Header.Target.Type = IPC_TYPE_MASTERDB;
	Request->AccountID = Client->Account.AccountID;
    Request->CharacterID = (Packet->CharacterIndex - Packet->CharacterIndex % MAX_CHARACTER_COUNT) / MAX_CHARACTER_COUNT;
    Request->CharacterIndex = Packet->CharacterIndex;
    IPCSocketUnicast(Server->IPCSocket, Request);
    return;

error:
	SocketDisconnect(Socket, Connection);
}

IPC_PROCEDURE_BINDING(D2W, GET_CHARACTER) {
    if (!ClientConnection || !Client) goto error;
    if (!Packet->Success) goto error;

    if (Context->Config.WorldSvr.DebugSetQuestFlags) {
        memset(Packet->CharacterData.QuestFlagInfo.FinishedQuests, 0xFF, RUNTIME_CHARACTER_MAX_QUEST_FLAG_COUNT);
    }

    Packet->CharacterData.Info.Profile.Nation = 3;
    /*
    Int32 SlotIndex = Packet->CharacterData.SkillSlotInfo.Count;
    for (Index Index = 144; Index < 148; Index++) {
        RTSkillSlotRef GmSkill = &Packet->CharacterData.SkillSlotInfo.Skills[Packet->CharacterData.SkillSlotInfo.Count];
        GmSkill->ID = Index;
        GmSkill->Level = 1;
        GmSkill->Index = SlotIndex++;
        Packet->CharacterData.SkillSlotInfo.Count += 1;
    }
    */
    /*
    RTWorldRef TargetWorld = RTRuntimeGetWorldByID(Runtime, Packet->CharacterData.CharacterInfo.Position.WorldID);
    if (TargetWorld->Type == RUNTIME_WORLD_TYPE_QUEST_DUNGEON) {
        RTDungeonDataRef QuestDungeonData = RTRuntimeGetDungeonDataByID(Runtime, TargetWorld->DungeonID);
        assert(QuestDungeonData);

        assert(0 <= QuestDungeonData->FailWarpNpcID - 1 && QuestDungeonData->FailWarpNpcID - 1 < Runtime->WarpIndexCount);
        RTWarpIndexRef WarpIndex = &Runtime->WarpIndices[QuestDungeonData->FailWarpNpcID - 1];
        RTPositionRef Position = &WarpIndex->Target[0];
        RTWorldRef NewWorld = RTRuntimeGetWorldByID(Runtime, WarpIndex->WorldID);
        assert(NewWorld);

        Packet->CharacterData.CharacterInfo.Position.X = Position->X;
        Packet->CharacterData.CharacterInfo.Position.Y = Position->Y;
        Packet->CharacterData.CharacterInfo.Position.WorldID = WarpIndex->WorldID;
    }
    */

    S2C_DATA_INITIALIZE* Response = PacketBufferInitExtended(ClientConnection->PacketBuffer, S2C, INITIALIZE);
    // TODO: Populate correct data!!!

    /* Server Info */
    Response->WorldType = Context->Config.WorldSvr.WorldType;
    Response->Server.ServerID = Context->Config.WorldSvr.GroupIndex;
    Response->Server.WorldServerID = Context->Config.WorldSvr.NodeIndex;
    Response->Server.PlayerCount = SocketGetConnectionCount(Context->ClientSocket);
    Response->Server.MaxPlayerCount = Context->Config.WorldSvr.MaxConnectionCount;
    memcpy(Response->Server.Address.Host, Context->Config.WorldSvr.Host, strlen(Context->Config.WorldSvr.Host));
    Response->Server.Address.Port = Context->Config.WorldSvr.Port;
    Response->Server.WorldType = Context->Config.WorldSvr.WorldType;

    /* Character Info */
    Response->WorldIndex = Packet->CharacterData.Info.Position.WorldID;
    Response->DungeonIndex = (UInt32)Packet->CharacterData.Info.Position.DungeonIndex;
    Response->Position.X = Packet->CharacterData.Info.Position.X;
    Response->Position.Y = Packet->CharacterData.Info.Position.Y;
    Response->Exp = Packet->CharacterData.Info.Basic.Exp;
    Response->Alz = Packet->CharacterData.Info.Currency[RUNTIME_CHARACTER_CURRENCY_ALZ];
    Response->Wexp = Packet->CharacterData.Info.Honor.Exp;
    Response->Level = Packet->CharacterData.Info.Basic.Level;
    Response->STR = Packet->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_STR];
    Response->DEX = Packet->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_DEX];
    Response->INT = Packet->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_INT];
    Response->PNT = Packet->CharacterData.Info.Stat[RUNTIME_CHARACTER_STAT_PNT];
    Response->SkillRank = Packet->CharacterData.Info.Skill.Rank;
    Response->SkillLevel = Packet->CharacterData.Info.Skill.Level;
    Response->SkillExp = (UInt32)Packet->CharacterData.Info.Skill.Exp;
    Response->SkillPoint = Packet->CharacterData.Info.Skill.Point;
    Response->RestExp = 0;
    Response->HonorPoint = Packet->CharacterData.Info.Honor.Point;

    memcpy(Response->ChatServerAddress.Host, Context->Config.ChatSvr.Host, strlen(Context->Config.ChatSvr.Host));
    Response->ChatServerAddress.Port = Context->Config.ChatSvr.Port;
    memcpy(Response->AuctionServerAddress.Host, Context->Config.AuctionSvr.Host, strlen(Context->Config.AuctionSvr.Host));
    Response->AuctionServerAddress.Port = Context->Config.AuctionSvr.Port;
    memcpy(Response->PartyServerAddress.Host, Context->Config.PartySvr.Host, strlen(Context->Config.PartySvr.Host));
    Response->PartyServerAddress.Port = Context->Config.PartySvr.Port;

    Response->Nation = Packet->CharacterData.Info.Profile.Nation;
    Response->WarpMask = Packet->CharacterData.Info.Profile.WarpMask;
    Response->MapsMask = Packet->CharacterData.Info.Profile.MapsMask;
    Response->CharacterStyle = SwapUInt32(Packet->CharacterData.Info.Style.RawValue);
    Response->CharacterLiveStyle = SwapUInt32(Packet->CharacterData.Info.LiveStyle.RawValue);
    Response->AP = Packet->CharacterData.Info.Ability.Point;
    Response->Axp = Packet->CharacterData.Info.Ability.Exp;
    Response->CharacterCreationDate = Packet->CharacterCreationDate;
    Response->ForceGem = (UInt32)Packet->CharacterData.Info.Currency[RUNTIME_CHARACTER_CURRENCY_GEM];
    Response->OverlordLevel = Packet->CharacterData.Info.Overlord.Level;
    Response->OverlordExp = Packet->CharacterData.Info.Overlord.Exp;
    Response->OverlordPoint = Packet->CharacterData.Info.Overlord.Point;
    // Response->SpecialGiftboxPoint = 0;
    Response->TranscendencePoint = 0;
    Response->NameLength = strlen(Packet->CharacterName) + 1;
    CString Name = (CString)PacketBufferAppend(ClientConnection->PacketBuffer, strlen(Packet->CharacterName));
    memcpy(Name, Packet->CharacterName, strlen(Packet->CharacterName));

    Response->EquipmentSlotCount = Packet->CharacterData.EquipmentInfo.Count;
    if (Packet->CharacterData.EquipmentInfo.Count > 0) {
        PacketBufferAppendCopy(
            ClientConnection->PacketBuffer,
            Packet->CharacterData.EquipmentInfo.Slots,
            sizeof(struct _RTItemSlot) * Packet->CharacterData.EquipmentInfo.Count
        );
    }

    Response->InventorySlotCount = Packet->CharacterData.InventoryInfo.Count;
    if (Packet->CharacterData.InventoryInfo.Count > 0) {
        PacketBufferAppendCopy(
            ClientConnection->PacketBuffer,
            Packet->CharacterData.InventoryInfo.Slots,
            sizeof(struct _RTItemSlot) * Packet->CharacterData.InventoryInfo.Count
        );
    }

    Response->SkillSlotCount = Packet->CharacterData.SkillSlotInfo.Count;
    if (Packet->CharacterData.SkillSlotInfo.Count > 0) {
        PacketBufferAppendCopy(
            ClientConnection->PacketBuffer,
            Packet->CharacterData.SkillSlotInfo.Skills,
            sizeof(struct _RTSkillSlot) * Packet->CharacterData.SkillSlotInfo.Count
        );
    }

    Response->QuickSlotCount = Packet->CharacterData.QuickSlotInfo.Count;
    if (Packet->CharacterData.QuickSlotInfo.Count > 0) {
        PacketBufferAppendCopy(
            ClientConnection->PacketBuffer,
            Packet->CharacterData.QuickSlotInfo.QuickSlots,
            sizeof(struct _RTQuickSlot) * Packet->CharacterData.QuickSlotInfo.Count
        );
    }

    Response->EssenceAbilityCount = Packet->CharacterData.EssenceAbilityInfo.Count;
    for (Int32 Index = 0; Index < Packet->CharacterData.EssenceAbilityInfo.Count; Index += 1) {
        RTEssenceAbilitySlotRef AbilitySlot = &Packet->CharacterData.EssenceAbilityInfo.Slots[Index];

        S2C_DATA_INITIALIZE_ESSENCE_ABILITY_SLOT* ResponseSlot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_INITIALIZE_ESSENCE_ABILITY_SLOT);
        ResponseSlot->AbilityID = AbilitySlot->AbilityID;
        ResponseSlot->Level = AbilitySlot->Level;
        ResponseSlot->Unknown1 = AbilitySlot->Unknown1;
    }

    Response->BlendedAbilityCount = Packet->CharacterData.BlendedAbilityInfo.Count;
    for (Int32 Index = 0; Index < Packet->CharacterData.BlendedAbilityInfo.Count; Index += 1) {
        RTBlendedAbilitySlotRef AbilitySlot = &Packet->CharacterData.BlendedAbilityInfo.Slots[Index];

        S2C_DATA_INITIALIZE_BLENDED_ABILITY_SLOT* ResponseSlot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_INITIALIZE_BLENDED_ABILITY_SLOT);
        ResponseSlot->AbilityID = AbilitySlot->AbilityID;
        ResponseSlot->Level = AbilitySlot->Level;
        ResponseSlot->Unknown1 = AbilitySlot->Unknown1;
    }

    Response->KarmaAbilityCount = Packet->CharacterData.KarmaAbilityInfo.Count;
    for (Int32 Index = 0; Index < Packet->CharacterData.KarmaAbilityInfo.Count; Index += 1) {
        RTKarmaAbilitySlotRef AbilitySlot = &Packet->CharacterData.KarmaAbilityInfo.Slots[Index];

        S2C_DATA_INITIALIZE_KARMA_ABILITY_SLOT* ResponseSlot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_INITIALIZE_KARMA_ABILITY_SLOT);
        ResponseSlot->AbilityID = AbilitySlot->AbilityID;
        ResponseSlot->Level = AbilitySlot->Level;
        ResponseSlot->Unknown1 = AbilitySlot->Unknown1;
    }

    memcpy(&Response->QuestFlagInfo, &Packet->CharacterData.QuestFlagInfo, sizeof(struct _RTCharacterQuestFlagInfo));
    memcpy(&Response->DungeonQuestFlagInfo, &Packet->CharacterData.DungeonQuestFlagInfo, sizeof(struct _RTCharacterDungeonQuestFlagInfo));

    Response->QuestSlotCount = 0;
    for (Int32 Index = 0; Index < RUNTIME_CHARACTER_MAX_QUEST_SLOT_COUNT; Index += 1) {
        RTQuestSlotRef QuestSlot = &Packet->CharacterData.QuestSlotInfo.QuestSlot[Index];
        if (!QuestSlot->QuestIndex) continue;

        RTQuestDataRef Quest = RTRuntimeGetQuestByIndex(Runtime, QuestSlot->QuestIndex);
        assert(Quest);

        UInt16 NpcActionFlags = 0;
        for (Int32 Index = 0; Index < QuestSlot->NpcActionIndex; Index += 1) {
            NpcActionFlags |= 1 << Index;
        }

        S2C_DATA_INITIALIZE_QUEST_INDEX* QuestResponse = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_INITIALIZE_QUEST_INDEX);
        QuestResponse->QuestIndex = QuestSlot->QuestIndex;
        QuestResponse->NpcActionFlags = NpcActionFlags;
        QuestResponse->DisplayNotice = QuestSlot->DisplayNotice;
        QuestResponse->DisplayOpenNotice = QuestSlot->DisplayOpenNotice;
        QuestResponse->QuestSlotIndex = Index;

        Int32 CounterCount = Quest->MissionMobCount + Quest->MissionItemCount;
        for (Int32 CounterIndex = 0; CounterIndex < CounterCount; CounterIndex++) {
            PacketBufferAppendValue(ClientConnection->PacketBuffer, UInt8, QuestSlot->Counter[CounterIndex]);
        }

        Response->QuestSlotCount += 1;
    }
    /*
    Response->PremiumServiceCount = 1;
    S2C_DATA_INITIALIZE_PREMIUM_SERVICE_SLOT* PremiumServiceSlot = PacketBufferAppendStruct(S2C_DATA_INITIALIZE_PREMIUM_SERVICE_SLOT);
    PremiumServiceSlot->ServiceIndex = 1;
    PremiumServiceSlot->Unknown1 = 0;
    PremiumServiceSlot->Duration.Year = 1;
    */
    Response->BlessingBeadCount = 0;
    Response->MercenaryCount = 0;
    Response->EquipmentAppearanceCount = 0;
    Response->InventoryAppearanceCount = 0;
    Response->AchievementCount = 0;
    Response->AchievementRewardCount = 0;
    Response->ExtendedAchievementRewardCount = 0;
    Response->CraftCount = 0;

    Response->RequestCraftCount = Packet->CharacterData.RequestCraftInfo.RequestCraftCount;
    Response->RequestCraftExp = Packet->CharacterData.RequestCraftInfo.RequestCraftExp;
    memcpy(Response->RequestCraftFlags, Packet->CharacterData.RequestCraftInfo.RequestCraftFlags, 1024);
    memcpy(Response->RequestCraftFavoriteFlags, Packet->CharacterData.RequestCraftInfo.RequestCraftFavoriteFlags, 1024);
    Response->RequestCraftSortOrder = Packet->CharacterData.RequestCraftInfo.RequestCraftSortOrder;

    Response->OverlordMasteryCount = Packet->CharacterData.OverlordMasteryInfo.Count;
    for (Int32 Index = 0; Index < Packet->CharacterData.OverlordMasteryInfo.Count; Index += 1) {
        S2C_DATA_OVERLORD_MASTERY_SLOT* MasterySlot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_OVERLORD_MASTERY_SLOT);
        MasterySlot->MasteryIndex = Packet->CharacterData.OverlordMasteryInfo.Slots[Index].MasteryIndex;
        MasterySlot->Level = Packet->CharacterData.OverlordMasteryInfo.Slots[Index].Level;
    }

    Response->UnknownMeritMasteryCount = 0;

    Response->HonorMedalExp = Packet->CharacterData.HonorMedalInfo.Score;
    Response->HonorMedalCount = Packet->CharacterData.HonorMedalInfo.SlotCount;
    for (Int32 Index = 0; Index < Packet->CharacterData.HonorMedalInfo.SlotCount; Index += 1) {
        S2C_DATA_HONOR_MEDAL_SLOT* Slot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_HONOR_MEDAL_SLOT);
        Slot->CategoryIndex = Packet->CharacterData.HonorMedalInfo.Slots[Index].CategoryIndex;
        Slot->GroupIndex = Packet->CharacterData.HonorMedalInfo.Slots[Index].GroupIndex;
        Slot->SlotIndex = Packet->CharacterData.HonorMedalInfo.Slots[Index].SlotIndex;
        Slot->ForceEffectIndex = Packet->CharacterData.HonorMedalInfo.Slots[Index].ForceEffectIndex;
        Slot->IsUnlocked = Packet->CharacterData.HonorMedalInfo.Slots[Index].IsUnlocked;
    }

    Response->ForceWingGrade = Packet->CharacterData.ForceWingInfo.Grade;
    Response->ForceWingLevel = Packet->CharacterData.ForceWingInfo.Level;
    Response->ForceWingExp = Packet->CharacterData.ForceWingInfo.Exp;
    Response->ForceWingUnknown1 = 0;
    Response->ForceWingActivePresetIndex = Packet->CharacterData.ForceWingInfo.ActivePresetIndex;

    for (Index Index = 0; Index < RUNTIME_CHARACTER_MAX_FORCE_WING_PRESET_PAGE_COUNT; Index += 1) {
        Response->ForceWingPresetEnabled[Index] = Packet->CharacterData.ForceWingInfo.PresetEnabled[Index];
        Response->ForceWingPresetTrainingPoints[Index] = Packet->CharacterData.ForceWingInfo.PresetTrainingPointCount[Index];
    }

    memcpy(
        &Response->ForceWingArrivalSkillSlots[0],
        &Packet->CharacterData.ForceWingInfo.ArrivalSkillSlots[0],
        sizeof(struct _RTForceWingArrivalSkillSlot) * RUNTIME_CHARACTER_MAX_FORCE_WING_ARRIVAL_SKILL_COUNT
    );
    
    memcpy(
        &Response->RestoreForceWingArrivalSkillSlot,
        &Packet->CharacterData.ForceWingInfo.ArrivalSkillRestoreSlot,
        sizeof(struct _RTForceWingArrivalSkillSlot)
    );

    for (Index Index = 0; Index < RUNTIME_CHARACTER_MAX_FORCE_WING_PRESET_PAGE_SIZE; Index += 1) {
        Response->ForceWingTrainingUnlockFlags[Index] = Packet->CharacterData.ForceWingInfo.TrainingUnlockFlags[Index];
    }
    
    Response->ForceWingPresetSlotCount = Packet->CharacterData.ForceWingInfo.PresetSlotCount;
    if (Packet->CharacterData.ForceWingInfo.PresetSlotCount > 0) {
        PacketBufferAppendCopy(
            ClientConnection->PacketBuffer,
            &Packet->CharacterData.ForceWingInfo.PresetSlots[0],
            sizeof(struct _RTForceWingPresetSlot) * Packet->CharacterData.ForceWingInfo.PresetSlotCount
        );
    }

    Response->ForceWingTrainingSlotCount = Packet->CharacterData.ForceWingInfo.TrainingSlotCount;
    if (Packet->CharacterData.ForceWingInfo.TrainingSlotCount > 0) {
        PacketBufferAppendCopy(
            ClientConnection->PacketBuffer,
            &Packet->CharacterData.ForceWingInfo.TrainingSlots[0],
            sizeof(struct _RTForceWingTrainingSlot)* Packet->CharacterData.ForceWingInfo.TrainingSlotCount
        );
    }

    Response->SpecialGiftboxCount = 5;
    for (Int32 Index = 0; Index < 5; Index += 1) {
        RTGiftBoxSlotRef GiftBox = PacketBufferAppendStruct(ClientConnection->PacketBuffer, struct _RTGiftBoxSlot);
        GiftBox->Index = Index;
        GiftBox->ReceivedCount = 333;
        GiftBox->ElapsedTime = 0;
        GiftBox->CooldownTime = 1;
        GiftBox->Active = 0;
    }

    Response->CollectionCount = Packet->CharacterData.CollectionInfo.Count;

    for (Int32 Index = 0; Index < Packet->CharacterData.CollectionInfo.Count; Index += 1) {
        S2C_DATA_COLLECTION_SLOT* CollectionSlot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_COLLECTION_SLOT);
        CollectionSlot->TypeID = Packet->CharacterData.CollectionInfo.Slots[Index].TypeID;
        CollectionSlot->CollectionID = Packet->CharacterData.CollectionInfo.Slots[Index].CollectionID;
        CollectionSlot->MissionID = Packet->CharacterData.CollectionInfo.Slots[Index].MissionID;
        CollectionSlot->ReceivedReward = Packet->CharacterData.CollectionInfo.Slots[Index].ReceivedReward;
        memcpy(&CollectionSlot->MissionItemCounts[0], &Packet->CharacterData.CollectionInfo.Slots[Index].MissionItemCounts[0], sizeof(UInt16) * RUNTIME_CHARACTER_MAX_COLLECTION_ITEM_COUNT);
    }

    Response->TransformCount = 0;
    Response->TranscendenceCount = 0;

    Response->NewbieSupportTimestamp = Packet->CharacterData.NewbieSupportInfo.Timestamp;
    Response->NewbieSupportSlotCount = Packet->CharacterData.NewbieSupportInfo.Count;
    for (Int32 Index = 0; Index < Packet->CharacterData.NewbieSupportInfo.Count; Index += 1) {
        S2C_DATA_NEWBIE_SUPPORT_SLOT* NewbieSupportSlot = PacketBufferAppendStruct(ClientConnection->PacketBuffer, S2C_DATA_NEWBIE_SUPPORT_SLOT);
        NewbieSupportSlot->CategoryType = Packet->CharacterData.NewbieSupportInfo.Slots[Index].CategoryType;
        NewbieSupportSlot->ConditionValue1 = Packet->CharacterData.NewbieSupportInfo.Slots[Index].ConditionValue1;
        NewbieSupportSlot->ConditionValue2 = Packet->CharacterData.NewbieSupportInfo.Slots[Index].ConditionValue2;
        NewbieSupportSlot->RewardIndex = Packet->CharacterData.NewbieSupportInfo.Slots[Index].RewardIndex;
    }

    assert(ClientConnection->ID < RUNTIME_MEMORY_MAX_CHARACTER_COUNT);

    Character = RTWorldManagerCreateCharacter(Context->Runtime->WorldManager, Packet->CharacterIndex);

    RTCharacterInitialize(
        Runtime, 
        Character,
        Packet->CharacterName,
        &Packet->CharacterData
    );

    Client->CharacterDatabaseID = Packet->CharacterID;
    Client->CharacterIndex = Packet->CharacterIndex;
    CStringCopySafe(Client->CharacterName, MAX_CHARACTER_NAME_LENGTH + 1, Packet->CharacterName);

    Response->Entity = Character->ID;

    RTWorldContextRef World = RTRuntimeGetWorldByCharacter(Runtime, Character);

    if ((World->WorldData->Type == RUNTIME_WORLD_TYPE_QUEST_DUNGEON ||
        World->WorldData->Type == RUNTIME_WORLD_TYPE_DUNGEON) &&
        World->DungeonIndex != Character->Data.Info.Position.DungeonIndex) {

        RTDungeonDataRef DungeonData = RTRuntimeGetDungeonDataByID(Runtime, Character->Data.Info.Position.DungeonIndex);
        if (!DungeonData) goto error;

        RTWarpPointResult WarpPoint = RTRuntimeGetWarpPoint(Runtime, Character, DungeonData->FailWarpNpcID);
        RTWorldContextRef TargetWorld = World;
        if (World->WorldData->WorldIndex != WarpPoint.WorldIndex) {
            TargetWorld = RTRuntimeGetWorldByID(Runtime, WarpPoint.WorldIndex);
            assert(TargetWorld);
        }

        World = TargetWorld;
        Character->Data.Info.Position.X = WarpPoint.X;
        Character->Data.Info.Position.Y = WarpPoint.Y;
        Character->Data.Info.Position.WorldID = WarpPoint.WorldIndex;
        Character->Data.Info.Position.DungeonIndex = TargetWorld->DungeonIndex;
        Response->WorldIndex = Character->Data.Info.Position.WorldID;
        Response->Position.X = Character->Data.Info.Position.X;
        Response->Position.Y = Character->Data.Info.Position.Y;
    }

    // TODO: Check if max hp or base hp is requested here...
    Response->BaseHP = Character->Attributes.Values[RUNTIME_ATTRIBUTE_HP_MAX];
    Response->MaxMP = (UInt32)Character->Attributes.Values[RUNTIME_ATTRIBUTE_MP_MAX];
    Response->MaxSP = Character->Attributes.Values[RUNTIME_ATTRIBUTE_SP_MAX];
    Response->MaxBP = (UInt32)Character->Attributes.Values[RUNTIME_ATTRIBUTE_BP_MAX];
    Response->MaxRage = (UInt32)Character->Attributes.Values[RUNTIME_ATTRIBUTE_RAGE_MAX];

    Response->CurrentHP = Character->Attributes.Values[RUNTIME_ATTRIBUTE_HP_CURRENT];
    Response->CurrentMP = (UInt32)Character->Attributes.Values[RUNTIME_ATTRIBUTE_MP_CURRENT];
    Response->CurrentSP = Character->Attributes.Values[RUNTIME_ATTRIBUTE_SP_CURRENT];
    Response->CurrentBP = (UInt32)Character->Attributes.Values[RUNTIME_ATTRIBUTE_BP_CURRENT];
    Response->CurrentRage = (UInt32)Character->Attributes.Values[RUNTIME_ATTRIBUTE_RAGE_CURRENT];

    /*
    Response->Unknown7[0] = 113;
    Response->Unknown7[1] = 201;
    Response->Unknown7[2] = 9;
    Response->UnknownSkillRank = Response->SkillRank + 1;
    Response->Unknown1[55] = 1;
    Response->Unknown1[58] = 20;
    */
    Response->MapsMask = 2101247;
    Response->WarpMask = 2101247;
    //memset(Response->DungeonFlagInfo.Flags, 0xFF, 640);

    /* this contains counters
    Int32 Seed = Character->Data.Info.Position.X;
    for (Int32 Index = 0; Index < 282; Index++)
        Response->Unknown14[Index] = Random(&Seed);
        */
    SocketSend(Context->ClientSocket, ClientConnection, Response);

    if (!RTCharacterIsAlive(Runtime, Character)) {
        RTWarpPointResult WarpPoint = RTRuntimeGetWarpPoint(Runtime, Character, World->WorldData->DeadWarpIndex);
        RTWorldContextRef TargetWorld = World;
        if (World->WorldData->WorldIndex != WarpPoint.WorldIndex) {
            TargetWorld = RTRuntimeGetWorldByID(Runtime, WarpPoint.WorldIndex);
            assert(TargetWorld);
        }
        
        Character->Data.Info.Resource.HP = Character->Attributes.Values[RUNTIME_ATTRIBUTE_HP_MAX];
        Character->Data.Info.Resource.MP = Character->Attributes.Values[RUNTIME_ATTRIBUTE_MP_MAX];
        Character->Data.Info.Position.X = WarpPoint.X;
        Character->Data.Info.Position.Y = WarpPoint.Y;
        Character->Data.Info.Position.WorldID = WarpPoint.WorldIndex;
        Character->Data.Info.Position.DungeonIndex = TargetWorld->DungeonIndex;
        RTCharacterInitializeAttributes(Runtime, Character);

        RTMovementInitialize(
            Runtime,
            &Character->Movement,
            WarpPoint.X,
            WarpPoint.Y,
            RUNTIME_MOVEMENT_SPEED_BASE,
            RUNTIME_WORLD_TILE_WALL
        );
    }

    RTWorldSpawnCharacter(Runtime, World, Character->ID);
    
    RTRuntimeDataEventHotReload(
        Context->Runtime->Context,
        Context->Config.WorldSvr.RuntimeDataPath,
        Context->Config.WorldSvr.ServerDataPath,
        "Event.xml"
    );

    // TODO: Move event data to database and trigger request on init
    SendEventInfo(Context, Client);
    SendEventList(Context, Client);
    SendPremiumServiceList(Context, Server->ClientSocket, Client->Connection);
    BroadcastUserList(Server, Context);

    IPC_W2P_DATA_CLIENT_CONNECT* Request = IPCPacketBufferInit(Server->IPCSocket->PacketBuffer, W2P, CLIENT_CONNECT);
    Request->Header.Source = Server->IPCSocket->NodeID;
    Request->Header.SourceConnectionID = Client->Connection->ID;
    Request->Header.Target.Group = Context->Config.WorldSvr.GroupIndex;
    Request->Header.Target.Type = IPC_TYPE_PARTY;
    Request->CharacterIndex = Character->CharacterIndex;
    IPCSocketUnicast(Server->IPCSocket, Request);
    return;

error:
	if (ClientConnection) {
		return SocketDisconnect(Context->ClientSocket, ClientConnection);
	}
}
