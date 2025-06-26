#pragma once

struct Color
{
	unsigned char red = 0, green = 0, blue = 0;
	float opacity = 0;
};

struct Line : Color
{
	float width = 2;
};

struct MeterRange : Color
{
	float low, high = 0;
};

struct MeterMarking : Line
{
	float value = 0;
};

void RenderMeter(
	CanvasWrapper canvas,
	Vector2F startPos,
	Vector2F boxSize,
	Color base,
	Line border,
	float totalUnits,
	std::list<MeterRange> ranges,
	std::list<MeterMarking> markings,
	bool vertical
);
