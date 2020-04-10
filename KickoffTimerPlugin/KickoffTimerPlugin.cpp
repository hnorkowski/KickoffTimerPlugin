#include "pch.h"
#include "KickoffTimerPlugin.h"


BAKKESMOD_PLUGIN(KickoffTimerPlugin, "Kickoff timer plugin", plugin_version, PLUGINTYPE_FREEPLAY)

static const string savefile = "bakkesmod/data/kickofftimerplugin.data";

void KickoffTimerPlugin::onLoad()
{
	spawnLocations.push_back({ "ML", {256, -3840}, 2.30312 });
	spawnLocations.push_back({ "MR", {-256, -3840}, 2.30312 });
	spawnLocations.push_back({ "M", {0, -4608}, 2.62897 });
	spawnLocations.push_back({ "L", {2048, -2560}, 2.06905 });
	spawnLocations.push_back({ "R", {-2048, -2560},  2.06905 });

	load();

	cvarManager->registerNotifier("kickofftimer_reset", [this](std::vector<string> params) {
		for (int i = 0; i < spawnLocations.size(); i++) {
			spawnLocations.at(i).personalBest = -1;
		}
		}, "Reset personal bests", PERMISSION_ALL);

	pDefaultTime = { "" };
	pBest = { "" };
	pBallHitted = { "" };
	pSupersonic = { "" };
	popups.push_back(&pDefaultTime);
	popups.push_back(&pBest);
	popups.push_back(&pBallHitted);
	popups.push_back(&pSupersonic);

	gameWrapper->HookEventPost("Function TAGame.Car_TA.EventHitBall", std::bind(&KickoffTimerPlugin::onHitBall, this, std::placeholders::_1));
	gameWrapper->HookEventPost("Function TAGame.Car_TA.OnSuperSonicChanged", std::bind(&KickoffTimerPlugin::onSupersonicChanged, this));
	gameWrapper->HookEventPost("Function Engine.Controller.Restart", std::bind(&KickoffTimerPlugin::onReset, this, std::placeholders::_1));
	//gameWrapper->HookEventPost("Function GameEvent_TrainingEditor_TA.Active.BeginState", std::bind(&KickoffTimerPlugin::onTrainingReset, this));
	gameWrapper->HookEventPost("Function TAGame.Car_TA.SetVehicleInput", std::bind(&KickoffTimerPlugin::onStartedDriving, this, std::placeholders::_1));
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
	if (timeHit < 1.5 || timeHit > 4)
		return;

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

	pBallHitted.text = string("Ball hitted after ") + to_string_with_precision(timeHit, 2) + string(" seconds.");
	lastMsg = chrono::system_clock::now();
}

void KickoffTimerPlugin::onSupersonicChanged()
{
	if (reachedSupersonic) return;
	auto car = gameWrapper->GetLocalCar();
	if (!car.IsNull())
	{
		if (car.GetbSuperSonic())
		{
			timeSupersonic = gameWrapper->GetGameEventAsServer().GetSecondsElapsed() - timeStart;
			reachedSupersonic = true;
			pSupersonic.text = "Reached supersonic after " + to_string_with_precision(timeSupersonic, 2) + string(" seconds.");
		}
		
	}
}

void KickoffTimerPlugin::onStartedDriving(std::string eventName)
{
	if (!gameWrapper->IsInGame() || started)
		return;
	CarWrapper car = gameWrapper->GetLocalCar();
	if (!car.IsNull()) {
		ControllerInput controllerInput = car.GetInput();
		if (controllerInput.Throttle > 0 || controllerInput.ActivateBoost > 0) {
			started = true;
			timeStart = gameWrapper->GetGameEventAsServer().GetSecondsElapsed();
		}
	}

}

void KickoffTimerPlugin::onReset(std::string eventName)
{
	if (!gameWrapper->IsInGame())
	{
		return;

	}
	started = false;
	hitted = false;
	reachedSupersonic = false;
	pBallHitted.text = "";
	pSupersonic.text = "";


	spawn = getSpawnLocation();
	if (spawn != 0) {
		pDefaultTime.text = spawn->name + string(" normal Kickoff time: ") + to_string_with_precision(spawn->normalTime, 2) + string(" seconds");

		if (spawn->personalBest > 0) {
			pBest.text = string("Personal Best: ") + to_string_with_precision(spawn->personalBest, 2) + string(" seconds");
		}
		else {
			pBest.text = "";
		}
		lastMsg = chrono::system_clock::now();
	}
}

void KickoffTimerPlugin::Render(CanvasWrapper canvas)
{
	if (!gameWrapper->IsInGame() || popups.empty() || !spawn != 0 || chrono::duration_cast<std::chrono::seconds> (chrono::system_clock::now() - lastMsg).count() > 4)
		return;

	auto screenSize = canvas.GetSize();
	for (int i = 0; i < popups.size(); i++)
	{
		auto pop = popups.at(i);
		if (pop->startLocation.X < 0)
		{
			pop->startLocation = { (int)(screenSize.X * 0.35), (int)(screenSize.Y * 0.1 + i * 0.035 * screenSize.Y) };
		}

		Vector2 drawLoc = { pop->startLocation.X, pop->startLocation.Y };
		canvas.SetPosition(drawLoc);
		canvas.SetColor(pop->color.R, pop->color.G, pop->color.B, 255);
		canvas.DrawString(pop->text, 3, 3);
	}
}

SpawnLocation* KickoffTimerPlugin::getSpawnLocation() {
	auto location = gameWrapper->GetLocalCar().GetLocation();
	cvarManager->log("x: " + to_string(location.X) + " y: " + to_string(location.Y));
	for (int i = 0; i < spawnLocations.size(); i++)
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

//void KickoffTimerPlugin::onTrainingReset()
//{
//	cvarManager->log("training reset event!");
//	bool trainingpack = IsInKickoffTrainingPack();
//	if (trainingpack)
//	{
//		cvarManager->log("In kickoff training pack");
//		auto car = gameWrapper->GetLocalCar();
//		auto player = gameWrapper->GetPlayerController();
//		if (!car.IsNull())
//		{
//			auto boost = car.GetBoostComponent();
//			if (!boost.IsNull())
//			{
//				cvarManager->log("limiting boost!");
//				boost.SetBoostAmount(0.33);
//			}
//			else {
//				cvarManager->log("boostwrapper is null");
//			}
//		}
//		else {
//			cvarManager->log("car is null");
//		}
//	}
//}

bool KickoffTimerPlugin::IsInKickoffTrainingPack()
{
	auto server = gameWrapper->GetGameEventAsServer();
	if (!server.IsNull())
	{
		//cvarManager->log("got server");
		auto player = server.GetLocalPrimaryPlayer();
		if (!player.IsNull())
		{
			auto mapURL = player.GetLoginURL().ToString();
			return mapURL.find("39F5BCED4EE8050310D28797FCD1641E") != std::string::npos;

		}
	}
	return false;
}

