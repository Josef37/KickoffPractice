#pragma once

struct Color
{
	unsigned char red = 0, green = 0, blue = 0;
	float opacity = 0;
};

struct Line : Color
{
	int width = 2;
};

struct MeterRange : Color
{
	float low, high = 0;
};

struct MeterMarking : Line
{
	float value = 0;
};

Vector2 RenderMeter(
	CanvasWrapper canvas,
	Vector2 startPos,
	Vector2 reqBoxSize,
	Color base,
	Line border,
	int totalUnits,
	std::list<MeterRange> ranges,
	std::list<MeterMarking> markings,
	bool vertical
);
