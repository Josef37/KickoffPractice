#include "pch.h"
#include "RenderMeter.h"

// `CanvasWrapper::DrawBox` always draws 2 pixels of border.
// This functions draws a border out of four non-overlapping boxes.
static void DrawBox(CanvasWrapper canvas, Vector2F innerSize, float width)
{
	auto position = canvas.GetPositionFloat();

	// top (with corner)
	canvas.SetPosition(position + Vector2F(-width, -width));
	canvas.FillBox(Vector2F(innerSize.X + 2 * width, width));

	// bottom (with corner)
	canvas.SetPosition(position + Vector2F(-width, innerSize.Y));
	canvas.FillBox(Vector2F(innerSize.X + 2 * width, width));

	// left
	canvas.SetPosition(position + Vector2F(-width, 0));
	canvas.FillBox(Vector2F(width, innerSize.Y));

	// right
	canvas.SetPosition(position + Vector2F(innerSize.X, 0));
	canvas.FillBox(Vector2F(width, innerSize.Y));

	canvas.SetPosition(position);
}

void RenderMeter(
	CanvasWrapper canvas,
	Vector2F startPos,
	Vector2F boxSize,
	Color base,
	Line border,
	Bounds bounds,
	std::list<MeterRange> ranges,
	std::list<MeterMarking> markings,
	bool vertical
)
{
	float unitWidth = (vertical ? boxSize.Y : boxSize.X) / (bounds.high - bounds.low);

	// Draw base meter base color
	canvas.SetColor(base.red, base.green, base.blue, (char)(255 * base.opacity));
	canvas.SetPosition(startPos);
	canvas.FillBox(boxSize);

	// Draw meter ranges
	for (const MeterRange& range : ranges)
	{
		auto low = std::clamp(range.low, bounds.low, bounds.high) - bounds.low;
		auto high = std::clamp(range.high, bounds.low, bounds.high) - bounds.low;

		if (low >= high) continue;

		canvas.SetColor(range.red, range.green, range.blue, (char)(255 * range.opacity));

		if (vertical)
		{
			auto position = Vector2F(startPos.X, startPos.Y + boxSize.Y - (high * unitWidth));
			auto size = Vector2F(boxSize.X, (high - low) * unitWidth);
			canvas.SetPosition(position);
			canvas.FillBox(size);
		}
		else
		{
			auto position = Vector2F(startPos.X + (low * unitWidth), startPos.Y);
			auto size = Vector2F((high - low) * unitWidth, boxSize.Y);
			canvas.SetPosition(position);
			canvas.FillBox(size);
		}
	}

	// Draw meter markings
	for (const MeterMarking& marking : markings)
	{
		if (marking.value < bounds.low || marking.value > bounds.high) continue;

		auto value = marking.value - bounds.low;

		canvas.SetColor(marking.red, marking.green, marking.blue, (char)(255 * marking.opacity));

		if (vertical)
		{
			auto position = Vector2F(startPos.X, startPos.Y + boxSize.Y - (value * unitWidth) - (marking.width / 2.f));
			auto size = Vector2F(boxSize.X, marking.width);
			canvas.SetPosition(position);
			canvas.FillBox(size);
		}
		else
		{
			auto position = Vector2F(startPos.X + (value * unitWidth) - (marking.width / 2.f), startPos.Y);
			auto size = Vector2F(marking.width, boxSize.Y);
			canvas.SetPosition(position);
			canvas.FillBox(size);
		}
	}

	// Draw meter border
	if (border.width > 0)
	{
		canvas.SetColor(border.red, border.green, border.blue, (char)(255 * border.opacity));
		canvas.SetPosition(startPos);
		DrawBox(canvas, boxSize, border.width);
	}
}
