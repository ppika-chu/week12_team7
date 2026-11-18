#pragma once

#include "Property.h"
#include "Object.h"
#include "Function.h"

using ClassConstructor = UObject* (*)(void* Memory);

class UClass : public UObject
{
public:
	UClass();

	FString Name;
	UClass* Super = nullptr;
	uint64 ClassSize = 0;
	ClassConstructor Constructor = nullptr;

	TArray<FProperty> Properties;
	UObject* DefaultObject = nullptr;
	// 클래스 설명. 에디터가 툴팁으로 보인다 (UE UCLASS 위 주석 = ToolTip 메타데이터)
	FString Tooltip;

	// 바로 앞에 등록한 프로퍼티에 설명을 붙인다. REFLECT 블록에서 PROPERTY_TOOLTIP으로 쓴다
	void SetLastPropertyTooltip(const char* InTooltip)
	{
		if (!Properties.IsEmpty())
		{
			Properties[Properties.Num() - 1].Tooltip = InTooltip;
		}
	}

	bool IsChildOf(const UClass* BaseClass) const;
	UObject* GetDefaultObject();

	inline const TArray<FProperty>& GetProperties() const { return Properties; }

	// 클래스 초기화 중에만 등록한다. 같은 이름은 파생 클래스의 등록을 우선한다.
	template<class T, void (T::*Method)()>
	void AddFunction(const FName& InName)
	{
		Functions.Add({ InName, [](UObject* Object)
		{
			(static_cast<T*>(Object)->*Method)();
		} });
	}
	const FFunction* FindFunctionByName(const FName& FunctionName) const;

	template<class T>
	T* GetDefaultObject()
	{
		return CastChecked<T>(GetDefaultObject());
	}

	template<typename T>
	void AddProperty(const FString& InName, uint64 InOffset, uint32 InFlags = CPF_Default)
	{
		UClass* ObjClass = nullptr;
		if constexpr (std::is_pointer_v<T> &&
			std::is_base_of_v<UObject, std::remove_pointer_t<T>>)
		{
			ObjClass = std::remove_pointer_t<T>::StaticClass();
		}

		Properties.Add({ InName, GetPropertyType<T>(), InOffset, sizeof(T), ObjClass, InFlags });
	}

	template<typename T>
	void AddProperty(const FString& InName, uint64 InOffset, EPropertyType InType, uint32 InFlags = CPF_Default)
	{
		Properties.Add({ InName, InType, InOffset, sizeof(T), nullptr, InFlags });
	}

	template<typename T>
	void AddEnumProperty(const FString& InName, uint64 InOffset, std::initializer_list<FString> InEnumNames, uint32 InFlags = CPF_Default)
	{
		// 직렬화와 Details가 int32로 읽고 쓴다
		static_assert(std::is_enum_v<T> && sizeof(T) == sizeof(int32), "PROPERTY_ENUM needs an int32-sized enum");
		FProperty Property{ InName, EPropertyType::Enum, InOffset, sizeof(T), nullptr, InFlags };
		Property.EnumNames = InEnumNames;
		Properties.Add(std::move(Property));
	}

private:
	TArray<FFunction> Functions;
	mutable int32 ClassUnique = 0;
	friend FName MakeUniqueObjectName(UObject*, const UClass*, FName);
};

// 보류
inline void CopyProperties(UObject* Src, UObject* Dst, UClass* FromClass)
{
	for (UClass* c = FromClass; c; c = c->Super)
	{
		for (const FProperty& p : c->Properties)
		{
			memcpy((char*)Dst + p.Offset, (char*)Src + p.Offset, p.Size);
		}
	}
}
