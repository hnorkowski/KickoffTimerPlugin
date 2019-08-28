#include "KickoffTimerPlugin.h"
#include "bakkesmod\wrappers\includes.h"
#include "utils/parser.h"
#include <string>

BAKKESMOD_PLUGIN(KickoffTimerPlugin, "Kickoff timer plugin", "0.1", PLUGINTYPE_FREEPLAY)


void KickoffTimerPlugin::onLoad()
{
	cvarManager->registerCvar("kickofftimer_personalBest_R", "-1", "Personal best data", false, false, 0.0, false, 0.0, true);
	cvarManager->registerCvar("kickofftimer_personalBest_L", "-1", "Personal best data", false);
	cvarManager->registerCvar("kickofftimer_personalBest_M", "-1", "Personal best data", false);
	cvarManager->registerCvar("kickofftimer_personalBest_MR", "-1", "Personal best data", false);
	cvarManager->registerCvar("kickofftimer_personalBest_ML", "-1", "Personal best data", false);

	cvarManager->log(to_string(cvarManager->getCvar("kickofftimer_personalBest_R").getFloatValue()));
	cvarManager->log(to_string(cvarManager->getCvar("kickofftimer_personalBest_L").getFloatValue()));
	cvarManager->log(to_string(cvarManager->getCvar("kickofftimer_personalBest_M").getFloatValue()));
	cvarManager->log(to_string(cvarManager->getCvar("kickofftimer_personalBest_MR").getFloatValue()));
	cvarManager->log(to_string(cvarManager->getCvar("kickofftimer_personalBest_ML").getFloatValue()));

	spawnLocations.push_back({ "ML", {256, -3840}, 2.30312, cvarManager->getCvar("kickofftimer_personalBest_ML").getFloatValue() });
	spawnLocations.push_back({ "MR", {-256, -3840}, 2.30312, cvarManager->getCvar("kickofftimer_personalBest_MR").getFloatValue() });
	spawnLocations.push_back({ "M", {0, -4608}, 2.62897, cvarManager->getCvar("kickofftimer_personalBest_M").getFloatValue() });
	spawnLocations.push_back({ "L", {2048, -2560}, 2.06905, cvarManager->getCvar("kickofftimer_personalBest_L").getFloatValue() });
	spawnLocations.push_back({ "R", {-2048, -2560},  2.06905, cvarManager->getCvar("kickofftimer_personalBest_R").getFloatValue() });

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
	gameWrapper->UnhookEvent("Function TAGame.FXActor_Car_TA.SetBraking");
	gameWrapper->UnhookEvent("Function Engine.Controller.Restart");
	gameWrapper->UnhookEvent("Function TAGame.Car_TA.EventHitBall");

	cvarManager->getCvar("kickofftimer_personalBest_ML").setValue(spawnLocations.at(0).personalBest);
	cvarManager->getCvar("kickofftimer_personalBest_MR").setValue(spawnLocations.at(1).personalBest);
	cvarManager->getCvar("kickofftimer_personalBest_M").setValue(spawnLocations.at(2).personalBest);
	cvarManager->getCvar("kickofftimer_personalBest_L").setValue(spawnLocations.at(3).personalBest);
	cvarManager->getCvar("kickofftimer_personalBest_R").setValue(spawnLocations.at(4).personalBest);
	cvarManager->log("Data saved!");
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
	//gameWrapper->LogToChatbox(msg, "Kickoff Timer");
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

		/*os.str("");
		os.clear();
		os << "Spawn Location: " << spawn->name << " - X:" << spawn->location.X << " Y:" << spawn->location.Y;
		msg = os.str();
		gameWrapper->LogToChatbox(msg, "Kickoff Timer");*/
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