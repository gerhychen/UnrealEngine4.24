// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraColorTypeEditorUtilities.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorStyle.h"
#include "Engine/Engine.h"

#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SGridPanel.h"

class SNiagaraColorParameterEditor : public SNiagaraParameterEditor
{
public:
	SLATE_BEGIN_ARGS(SNiagaraColorParameterEditor) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs)
	{
		SNiagaraParameterEditor::Construct(SNiagaraParameterEditor::FArguments()
			.MinimumDesiredWidth(3 * DefaultInputSize)
			.MaximumDesiredWidth(3 * DefaultInputSize));

		ChildSlot
		[
			SNew(SGridPanel)
			.FillColumn(1, 1)
			.FillColumn(2, 1)
			.FillColumn(3, 1)
			+ SGridPanel::Slot(0, 0)
			.RowSpan(2)
			[
				SAssignNew(ColorBlock, SColorBlock)
				.Color(this, &SNiagaraColorParameterEditor::GetColor)
				.ShowBackgroundForAlpha(true)
				.IgnoreAlpha(false)
				.OnMouseButtonDown(this, &SNiagaraColorParameterEditor::OnMouseButtonDownColorBlock)
				.Size(FVector2D(35.0f, 12.0f))
			]
			+ SGridPanel::Slot(1, 0)
			.Padding(3, 0, 0, 0)
			[
				ConstructComponentWidget(0, NSLOCTEXT("ColorParameterEditor", "RLabel", "R"))
			]
			+ SGridPanel::Slot(2, 0)
			.Padding(3, 0, 0, 0)
			[
				ConstructComponentWidget(1, NSLOCTEXT("ColorParameterEditor", "GLabel", "G"))
			]
			+ SGridPanel::Slot(3, 0)
			.Padding(3, 0, 0, 0)
			[
				ConstructComponentWidget(2, NSLOCTEXT("ColorParameterEditor", "BLabel", "B"))
			]
			+ SGridPanel::Slot(1, 1)
			.Padding(3, 2, 0, 0)
			[
				ConstructComponentWidget(3, NSLOCTEXT("ColorParameterEditor", "ALabel", "A"))
			]
		];
	}

	virtual void UpdateInternalValueFromStruct(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetColorStruct(), TEXT("Struct type not supported."));
		ColorValue = *((FLinearColor*)Struct->GetStructMemory());
	}

	virtual void UpdateStructFromInternalValue(TSharedRef<FStructOnScope> Struct) override
	{
		checkf(Struct->GetStruct() == FNiagaraTypeDefinition::GetColorStruct(), TEXT("Struct type not supported."));
		*((FLinearColor*)Struct->GetStructMemory()) = ColorValue;
	}

	virtual bool CanChangeContinuously() const override { return true; }

private:
	TSharedRef<SWidget> ConstructComponentWidget(int32 Index, FText ComponentLabel)
	{
		return SNew(SNumericEntryBox<float>)
		.Font(FNiagaraEditorStyle::Get().GetFontStyle("NiagaraEditor.ParameterFont"))
		.OverrideTextMargin(2)
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.Delta(0.0f)
		.Value(this, &SNiagaraColorParameterEditor::GetComponentValue, Index)
		.OnValueChanged(this, &SNiagaraColorParameterEditor::ComponentValueChanged, Index)
		.OnValueCommitted(this, &SNiagaraColorParameterEditor::ComponentValueCommitted, Index)
		.OnBeginSliderMovement(this, &SNiagaraColorParameterEditor::BeginSliderMovement)
		.OnEndSliderMovement(this, &SNiagaraColorParameterEditor::EndSliderMovement)
		.AllowSpin(true)
		.LabelVAlign(EVerticalAlignment::VAlign_Center)
		.Label()
		[
			SNew(STextBlock)
			.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
			.Text(ComponentLabel)
		];
	}

	void BeginSliderMovement()
	{
		ExecuteOnBeginValueChange();
	}

	void EndSliderMovement(float Value)
	{
		ExecuteOnEndValueChange();
	}

	TOptional<float> GetComponentValue(int32 Index) const
	{
		return TOptional<float>(ColorValue.Component(Index));
	}

	void ComponentValueChanged(float ComponentValue, int32 Index)
	{
		ColorValue.Component(Index) = ComponentValue;
		ExecuteOnValueChanged();
	}

	void ComponentValueCommitted(float ComponentValue, ETextCommit::Type CommitInfo, int32 Index)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			ComponentValueChanged(ComponentValue, Index);
		}
	}

	FReply OnMouseButtonDownColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		FColorPickerArgs PickerArgs;
		{
			PickerArgs.bUseAlpha = true;
			PickerArgs.bOnlyRefreshOnMouseUp = false;
			PickerArgs.bOnlyRefreshOnOk = false;
			PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
			PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SNiagaraColorParameterEditor::SetColor);
			PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &SNiagaraColorParameterEditor::ColorPickerCancelled);
			PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &SNiagaraColorParameterEditor::InteractivePickBegin);
			PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &SNiagaraColorParameterEditor::InteractivePickEnd);
			PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &SNiagaraColorParameterEditor::ColorPickerClosed);
			PickerArgs.InitialColorOverride = ColorValue;
			PickerArgs.ParentWidget = ColorBlock;
		}

		OpenColorPicker(PickerArgs);
		// Mark this parameter editor as editing exclusively so that the corresponding structure details view doesn't get updated
		// since it closes all color pickers when it gets updated!
		SetIsEditingExclusively(true);
		return FReply::Handled();
	}

	void InteractivePickBegin()
	{
		ExecuteOnBeginValueChange();
	}

	void InteractivePickEnd()
	{
		ExecuteOnEndValueChange();
	}

	void ColorPickerCancelled(FLinearColor OriginalColor)
	{
		ColorValue = OriginalColor;
		ExecuteOnValueChanged();
	}

	void ColorPickerClosed(const TSharedRef<SWindow>& Window)
	{
		SetIsEditingExclusively(false);
	}

	FLinearColor GetColor() const
	{
		return ColorValue;
	}

	void SetColor(FLinearColor NewColor)
	{
		ColorValue = NewColor;
		ExecuteOnValueChanged();
	}

private:
	TSharedPtr<SColorBlock> ColorBlock;

	FLinearColor ColorValue;
};

void FNiagaraEditorColorTypeUtilities::UpdateVariableWithDefaultValue(FNiagaraVariable& Variable) const
{
	checkf(Variable.GetType().GetStruct() == FNiagaraTypeDefinition::GetColorStruct(), TEXT("Struct type not supported."));
	Variable.SetValue<FLinearColor>(FLinearColor(1, 1, 1, 1));
}

TSharedPtr<SNiagaraParameterEditor> FNiagaraEditorColorTypeUtilities::CreateParameterEditor(const FNiagaraTypeDefinition& ParameterType) const
{
	return SNew(SNiagaraColorParameterEditor);
}

bool FNiagaraEditorColorTypeUtilities::CanHandlePinDefaults() const
{
	return true;
}

FString FNiagaraEditorColorTypeUtilities::GetPinDefaultStringFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	checkf(AllocatedVariable.IsDataAllocated(), TEXT("Can not generate a default value string for an unallocated variable."));
	return AllocatedVariable.GetValue<FLinearColor>().ToString();
}

bool FNiagaraEditorColorTypeUtilities::SetValueFromPinDefaultString(const FString& StringValue, FNiagaraVariable& Variable) const
{
	FLinearColor ColorValue;
	if (ColorValue.InitFromString(StringValue))
	{
		Variable.SetValue<FLinearColor>(ColorValue);
		return true;
	}
	return false;
}

FText FNiagaraEditorColorTypeUtilities::GetSearchTextFromValue(const FNiagaraVariable& AllocatedVariable) const
{
	return FText::FromString(GetPinDefaultStringFromValue(AllocatedVariable));
}
