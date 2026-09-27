// Blueprint-facing mirrors of the rules core enums and shared structs.
// The enum orders MUST match DarkBloodRules (static_asserts in DBRulesBridge.cpp).
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "DBTypes.generated.h"

UENUM(BlueprintType)
enum class EDBItemCategory : uint8
{
	Weapon,
	Armor,
	Bag,
	Material,
	Food,
	Potion,
	Scroll,
	Magic,
	Consumable,
	Quest,
	Misc,
};

UENUM(BlueprintType)
enum class EDBItemRarity : uint8
{
	Common UMETA(DisplayName = "Gewoehnlich"),
	Uncommon UMETA(DisplayName = "Ungewoehnlich"),
	Rare UMETA(DisplayName = "Selten"),
	Epic UMETA(DisplayName = "Episch"),
	Legendary UMETA(DisplayName = "Legendaer"),
	Demonic UMETA(DisplayName = "Daemonisch"),
};

UENUM(BlueprintType)
enum class EDBEquipSlot : uint8
{
	None,
	MainHand,
	OffHand,
	Head,
	Chest,
	Hands,
	Legs,
	Feet,
	Accessory1,
	Accessory2,
};

UENUM(BlueprintType)
enum class EDBBagKind : uint8
{
	General UMETA(DisplayName = "Reisetasche / Rucksack"),
	Materials UMETA(DisplayName = "Materialtasche"),
	Provisions UMETA(DisplayName = "Provianttasche"),
	Scrolls UMETA(DisplayName = "Schriftrollentasche"),
	Loot UMETA(DisplayName = "Beutetasche"),
};

UENUM(BlueprintType)
enum class EDBDangerTier : uint8
{
	Trivial UMETA(DisplayName = "Gering"),
	Appropriate UMETA(DisplayName = "Angemessen"),
	Challenging UMETA(DisplayName = "Herausfordernd"),
	Dangerous UMETA(DisplayName = "Gefaehrlich"),
	Extreme UMETA(DisplayName = "Extrem"),
};

UENUM(BlueprintType)
enum class EDBObjectiveKind : uint8
{
	Kill,
	Collect,
	Reach,
	Talk,
	Interact,
	Custom,
};

UENUM(BlueprintType)
enum class EDBQuestCategory : uint8
{
	Main,
	Side,
	Class,
	Dungeon,
	Bounty,
	Regional,
	Hidden,
	Dynamic,
	Settlement,
};

UENUM(BlueprintType)
enum class EDBQuestScope : uint8
{
	Shared,
	Personal,
};

UENUM(BlueprintType)
enum class EDBQuestStatus : uint8
{
	Inactive,
	Active,
	ReadyToTurnIn,
	Completed,
	Failed,
};

UENUM(BlueprintType)
enum class EDBRegionControl : uint8
{
	Occupied,
	Contested,
	Liberated,
};

UENUM(BlueprintType)
enum class EDBRegionKind : uint8
{
	Capital,
	VassalRegion,
	FinalRegion,
	Epilogue,
};

UENUM(BlueprintType)
enum class EDBBodyType : uint8
{
	TypeA,
	TypeB,
};

UENUM(BlueprintType)
enum class EDBBossRank : uint8
{
	WorldBoss,
	MidBoss,
	Vassal,
	DemonKing,
};

UENUM(BlueprintType)
enum class EDBPersistenceMode : uint8
{
	/** Listen server / co-op: every player keeps characters locally and uploads them on join (validated by the server). */
	LocalCharacters,
	/** Dedicated server: the server owns and stores all characters. */
	ServerAuthoritative,
};

USTRUCT(BlueprintType)
struct FDBItemGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = 1))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FDBMorphValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FName Morph;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = -1, ClampMax = 1))
	float Value = 0.f;
};

/** Visual character customization. Purely cosmetic: never read by gameplay code. */
USTRUCT(BlueprintType)
struct FDBAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	EDBBodyType BodyType = EDBBodyType::TypeA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 FacePreset = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 SkinTone = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 SkinDetail = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 HairStyle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FColor HairColor = FColor(26, 26, 26);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 EyeStyle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FColor EyeColor = FColor(59, 42, 30);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TArray<int32> Scars;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TArray<FDBMorphValue> Morphs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 VoicePreset = 0;
};

/** Public identity of a character; replicated to every player. */
USTRUCT(BlueprintType)
struct FDBCharacterProfile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character")
	FGuid CharacterId;

	/** Freely chosen character name - independent of the platform/account name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FName ClassId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FDBAppearance Appearance;
};

/** Reference to one inventory slot (see DarkBlood::Rules::FInventory for the section layout). */
USTRUCT(BlueprintType)
struct FDBSlotRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Section = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Index = 0;
};
