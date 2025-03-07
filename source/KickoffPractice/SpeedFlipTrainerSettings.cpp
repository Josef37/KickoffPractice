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

	CVarWrapper leftAngleCvar = cvarManager->getCvar(CVAR_LEFT_ANGLE);
	if (!leftAngleCvar) return;

	int leftAngle = leftAngleCvar.getIntValue();
	if (ImGui::SliderInt("Optimal left angle", &leftAngle, -70, -15, "%d degrees"))
		leftAngleCvar.setValue(leftAngle);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The optimal angle at which to dodge left.");

	CVarWrapper rightAngleCvar = cvarManager->getCvar(CVAR_RIGHT_ANGLE);
	if (!rightAngleCvar) return;

	int rightAngle = rightAngleCvar.getIntValue();
	if (ImGui::SliderInt("Optimal right angle", &rightAngle, 15, 70, "%d degrees"))
		rightAngleCvar.setValue(rightAngle);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The optimal angle at which to dodge right.");

	// ------------------------ FLIP CANCEL ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_FLIP);
		if (!cvar) return;

		bool value = cvar.getBoolValue();

		if (ImGui::Checkbox("Show time to flip cancel", &value))
			cvar.setValue(value);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Show meter for the time to flip cancel.");
	}

	CVarWrapper cancelCvar = cvarManager->getCvar(CVAR_CANCEL_THRESHOLD);
	if (!cancelCvar) return;

	int cancel = cancelCvar.getIntValue();
	if (ImGui::SliderInt("Flip cancel threshold", &cancel, 0, 150, "%d ms"))
		cancelCvar.setValue(cancel);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Number of milliseconds to perform flip cancel under.");

	// ------------------------ FIRST JUMP ----------------------------------
	SpacedSeparator();
	{
		CVarWrapper cvar = cvarManager->getCvar(CVAR_SHOW_JUMP);
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
