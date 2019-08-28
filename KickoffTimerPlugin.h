#pragma once
#pragma comment( lib, "bakkesmod.lib" )
#include "bakkesmod/plugin/bakkesmodplugin.h"

struct Color
{
	unsigned char R;
	unsigned char G;
	unsigned char B;
};
struct Popup
{
	std::string text = "";
	Color color = { 255, 255, 255 };
	Vector2 startLocation = { -1, -1 };
	
};
struct SpawnLocation
{
	std::string name = "";
	Vector2 location = { 0, 0 };
	float normalTime = -1;
	float personalBest = -1;
};

class KickoffTimerPlugin : public BakkesMod::Plugin::BakkesModPlugin
{
private:
	float timeStart;
	float timeHit;
	bool hitted = false;
	std::vector<Popup*> popups;
	Popup pDefaultTime;
	Popup pBallHitted;
	Popup pBest;
	std::vector<SpawnLocation> spawnLocations;
	SpawnLocation* spawn;

public:
	virtual void onLoad();
	virtual void onUnload();
	virtual void onHitBall(std::string eventName);
	virtual void onStartedDriving(std::string eventName);
	virtual void onReset(std::string eventName);
	virtual void Render(CanvasWrapper canvas);
	virtual SpawnLocation* getSpawnLocation();
};