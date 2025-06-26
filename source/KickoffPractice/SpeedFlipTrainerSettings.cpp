#include "pch.h"
#include "SpeedFlipTrainer.h"

void SpacedSeparator()
{
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
}

void SpeedFlipTrainer::RenderSettings(std::string CVAR_ENABLE)
{
	// ----------------------- ENABLE -------------------------------
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_ENABLE);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show Speedflip Trainer", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show Speedflip Trainer overlay while training kickoffs.");
	}

	// ------------------------ ANGLE ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_ANGLE);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show dodge angle", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show meter for the dodge angle.");
	}

	CVarWrapper angleCvar = cvarManager->getCvar(CVAR_TARGET_ANGLE);
	if (!angleCvar) return;

	int angle = angleCvar.getIntValue();
	if (ImGui::SliderInt("Optimal dodge angle", &angle, 15, 70, "%d degrees"))
		angleCvar.setValue(angle);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The optimal angle in degrees at which to dodge.");

	// ------------------------ FLIP CANCEL ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_FLIP_CANCEL);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show time to flip cancel", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show meter for the time to flip cancel.");
	}

	CVarWrapper cancelCvar = cvarManager->getCvar(CVAR_FLIP_CANCEL_THRESHOLD);
	if (!cancelCvar) return;

	int cancel = cancelCvar.getIntValue();
	if (ImGui::SliderInt("Flip cancel threshold", &cancel, 0, 150, "%d ms"))
		cancelCvar.setValue(cancel);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Number of milliseconds to perform flip cancel under.");

	// ------------------------ FIRST JUMP ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_FIRST_JUMP);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show time to first jump", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show meter for time to first jump");
	}

	CVarWrapper jumpLowCVar = cvarManager->getCvar(CVAR_JUMP_LOW);
	if (!jumpLowCVar) return;
	CVarWrapper jumpHighCVar = cvarManager->getCvar(CVAR_JUMP_HIGH);
	if (!jumpHighCVar) return;

	int jumpLow = jumpLowCVar.getIntValue();
	int jumpHigh = jumpHighCVar.getIntValue();

	if (ImGui::DragIntRange2("Optimal initial jump timing", &jumpLow, &jumpHigh, 1.0f, 0, 1000, "%d ms"))
	{
		jumpLowCVar.setValue(jumpLow);
		jumpHighCVar.setValue(jumpHigh);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Number of milliseconds to perform the initial jump within.");

	// ------------------------ SECOND JUMP ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_SECOND_JUMP);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show time to second jump", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show meter for time between first and second jump");
	}

	CVarWrapper secondJumpCvar = cvarManager->getCvar(CVAR_SECOND_JUMP_THRESHOLD);
	if (!secondJumpCvar) return;

	int secondJump = secondJumpCvar.getIntValue();
	if (ImGui::SliderInt("Second jump threshold", &secondJump, 0, 250, "%d ms"))
		secondJumpCvar.setValue(secondJump);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Number of milliseconds to perform the second jump within.");

	// ------------------------ POSITION ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_POSITION);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show horizontal position", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show meter for the horizontal position.");
	}
}
