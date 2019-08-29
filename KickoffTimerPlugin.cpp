#include "KickoffTimerPlugin.h"
#include "bakkesmod\wrappers\includes.h"
#include "utils/parser.h"
#include <string>

BAKKESMOD_PLUGIN(KickoffTimerPlugin, "Kickoff timer plugin", "0.1", PLUGINTYPE_FREEPLAY)

static const string savefile = "bakkesmod/data/kickofftimerplugin.data";

void KickoffTimerPlugin::onLoad()
{
	spawnLocations.push_back({ "ML", {256, -3840}, 2.30312 });
	spawnLocations.push_back({ "MR", {-256, -3840}, 2.30312  });
	spawnLocations.push_back({ "M", {0, -4608}, 2.62897 });
	spawnLocations.push_back({ "L", {2048, -2560}, 2.06905 });
	spawnLocations.push_back({ "R", {-2048, -2560},  2.06905 });

	load();

	cvarManager->registerNotifier("kickofftimer_reset", [this](std::vector<string> params) {
		for (int i = 0; i < spawnLocations.size(); i++) {
			spawnLocations.at(i).personalBest = -1;
		}
	}, "Reset personal bests", PERMISSION_ALL);

	pDefaultTime = {""};
	pBest = { ""};
	pBallHitted = { ""};
	popups.push_back(&pDefaultTime);
	popups.push_back(&pBest);
	popups.push_back(&pBallHitted);

	gameWrapper->HookEventPost("Function TAGame.Car_TA.EventHitBall", std::bind(&KickoffTimerPlugin::onHitBall, this, std::placeholders::_1));
	gameWrapper->HookEventPost("Function Engine.Controller.Restart", std::bind(&KickoffTimerPlugin::onReset, this, std::placeholders::_1));
	gameWrapper->HookEventPost("Function GameEvent_Soccar_TA.Active.StartRound", std::bind(&KickoffTimerPlugin::onStartedDriving, this, std::placeholders::_1));
	gameWrapper->RegisterDrawable(std::bind(&KickoffTimerPlugin::Render, this, std::placeholders::_1));
}

void KickoffTimerPlugin::onUnload()
{
	save();
}

void KickoffTimerPlugin::onHitBall(std::string eventName) 
{
	if (!gameWrapper->IsInGame() || hitted || spawn == 0)
		return;

	timeHit = gameWrapper->GetGameEventAsServer().GetSecondsElapsed() - timeStart;
	hitted = true;

	if (spawn->personalBest == -1) 
	{
		spawn->personalBest = timeHit;
		pBallHitted.color = { 255,255,255 };
	}
	else
	{
		if (timeHit < spawn->personalBest)
		{
			pBallHitted.color = { 255,200,0 };
			spawn->personalBest = timeHit;
		}
		else {
			pBallHitted.color = { 255,0,0 };
		}
		
	}
	if (timeHit < spawn->normalTime) 
	{
		pBallHitted.color = { 0,255,0 };
	}

	std::ostringstream os;
	os << "Ball hitted after " << to_string_with_precision(timeHit, 2) << " seconds.";
	std::string msg = os.str();
	pBallHitted.text = msg;
}

void KickoffTimerPlugin::onStartedDriving(std::string eventName)
{
	if (!gameWrapper->IsInGame())
			return;

	timeStart = gameWrapper->GetGameEventAsServer().GetSecondsElapsed();
}

void KickoffTimerPlugin::onReset(std::string eventName)
{
	if (!gameWrapper->IsInGame())
		return;

	hitted = false;
	pBallHitted.text = "";
	
	spawn = getSpawnLocation();
	if (spawn != 0) {
		std::ostringstream os;
		os << spawn->name << " normal Kickoff time: " << to_string_with_precision(spawn->normalTime, 2) << " seconds";
		std::string msg = os.str();
		pDefaultTime.text = msg;

		os.str("");
		os.clear();
		if(spawn->personalBest > 0) os << "Personal Best: " << to_string_with_precision(spawn->personalBest, 2) << " seconds";
		msg = os.str();
		pBest.text = msg;
	}
}

void KickoffTimerPlugin::Render(CanvasWrapper canvas)
{
	if (!gameWrapper->IsInGame() || popups.empty() || !spawn != 0)
		return;

	auto screenSize = canvas.GetSize();
	for(int i = 0; i < popups.size(); i++)
	{
		auto pop = popups.at(i);
		if (pop->startLocation.X < 0)
		{
			pop->startLocation = {(int)(screenSize.X * 0.35), (int)(screenSize.Y * 0.1 + i * 0.035 * screenSize.Y)};
		}

		Vector2 drawLoc = { pop->startLocation.X, pop->startLocation.Y };
		canvas.SetPosition(drawLoc);
		canvas.SetColor(pop->color.R, pop->color.G, pop->color.B, 255);
		canvas.DrawString(pop->text, 3, 3);
	}
}

SpawnLocation* KickoffTimerPlugin::getSpawnLocation() {
	auto location = gameWrapper->GetLocalCar().GetLocation();
	for(int i = 0; i < spawnLocations.size(); i++)
	{
		auto it = &spawnLocations.at(i);
		if (location.X == it->location.X && location.Y == it->location.Y) {
			return it;
		}
	}
	return 0;
}

void KickoffTimerPlugin::save() {
	ofstream myfile;
	myfile.open(savefile);
	if (myfile.is_open())
	{
		for (int i = 0; i < spawnLocations.size(); i++)
		{
			myfile << spawnLocations.at(i).personalBest << "\n";
		}
	}
	else
	{
		cvarManager->log("Can't write savefile.");
	}
	myfile.close();
}
void KickoffTimerPlugin::load() {
	ifstream myfile;
	myfile.open(savefile);
	if (myfile.good())
	{
		float f;
		char buffer[8];

		for (int i = 0; i < spawnLocations.size(); i++)
		{
			myfile.getline(buffer, 8);
			if (!myfile.eof())
			{
				f = (float)atof(buffer);
				spawnLocations.at(i).personalBest = f;
			}
			else
			{
				cvarManager->log("End of savefile reached too early!");
				break;
			}
		}
	}
	else
	{
		cvarManager->log("Can't read savefile.");
	}
}
