#include "KickoffTimerPlugin.h"
#include "bakkesmod/wrappers/includes.h"
#include "utils/parser.h"
#include <string>
#include <stdio.h>
#include <filesystem>

BAKKESMOD_PLUGIN(KickoffTimerPlugin, "Kickoff timer plugin", "0.1", PLUGINTYPE_FREEPLAY)

string savefile = "bakkesmod/data/kickofftimerplugin.data";
enum event_types { normal, better_than_normal, new_pb, slow_time };
map<event_types, Color> color_mapping =
{
	{ normal,             { 255, 255, 255 }}, // White
	{ better_than_normal, { 255, 200, 0 }}, // Yellow-ish
	{ new_pb,             { 0, 255, 0 }}, // Green
	{ slow_time,          { 255, 0, 0 }} // Red
};

int DECIMAL_PRECISION;

void KickoffTimerPlugin::onLoad()
{
	spawnLocations.push_back({ "ML", {256, -3840}, 2.30312 });
	spawnLocations.push_back({ "MR", {-256, -3840}, 2.30312  });
	spawnLocations.push_back({ "M",  {0, -4608}, 2.62897 });
	spawnLocations.push_back({ "L",  {2048, -2560}, 2.06905 });
	spawnLocations.push_back({ "R",  {-2048, -2560},  2.06905 });

	loadPBFile();

	cvarManager->registerNotifier("kickofftimer_reset", [this](std::vector<string> params) {
		ResetAndDeleteFile(false);
	}, "Reset personal bests", PERMISSION_ALL);

	cvarManager->registerNotifier("kickofftimer_resetAndDelete", [this](std::vector<string> params) {
		ResetAndDeleteFile(true);
	}, "Reset personal bests and Delete PB file", PERMISSION_ALL);

	cvarManager->registerCvar("kickofftimer_decimalPlaces", "2", "How many decimal places to record time", true, true, 2, true, 4, true);
	cvarManager->getCvar("kickofftimer_decimalPlaces").addOnValueChanged(std::bind(&KickoffTimerPlugin::updateDecimalValue, this));

	DECIMAL_PRECISION = cvarManager->getCvar("kickofftimer_decimalPlaces").getIntValue();
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

void KickoffTimerPlugin::ResetAndDeleteFile(const bool deleteFile)
{
	for (auto& spawn_location : spawnLocations)
	{
		spawn_location.personalBest = -1;
	}

	if (deleteFile)
	{
		const auto path = experimental::filesystem::absolute(savefile);
		remove(path);
	}
}

void KickoffTimerPlugin::onUnload()
{
	savePBFile();
}

void KickoffTimerPlugin::onHitBall(std::string eventName) 
{
	if (!gameWrapper->IsInGame() || hitted || spawn == 0)
		return;

	const int decimalMultiplyer = pow(10, DECIMAL_PRECISION);

	timeHit = gameWrapper->GetGameEventAsServer().GetSecondsElapsed() - timeStart;
	timeHit = roundf(timeHit * decimalMultiplyer) / decimalMultiplyer; // Round to a certain decimal place
	cvarManager->log("timeHit: " + std::to_string(timeHit));

	hitted = true;

	const auto current_pb = spawn->personalBest;
	cvarManager->log("current_pb: " + std::to_string(current_pb));

	auto normal_time = spawn->normalTime;
	normal_time = roundf(normal_time * decimalMultiplyer) / decimalMultiplyer; // Round to a certain decimal place
	cvarManager->log("normal_time: " + std::to_string(normal_time));

	// First time, set it to PB
	if (current_pb == -1)
	{
		spawn->personalBest = timeHit;

		auto message = "First time! Ball hitted after " +
			to_string_with_precision(timeHit, DECIMAL_PRECISION) + " seconds.";
		SetHitTextWithTimeAndText(timeHit, message);
		return;
	}

	// If its above normal, thats slow my friend
	if (timeHit > normal_time)
	{
		pBallHitted.color = color_mapping[slow_time];
		SetHitTextWithTime(timeHit);
		return;
	}

	// If its a new pb, congrats!
	if (timeHit < current_pb)
	{
		spawn->personalBest = timeHit;
		pBallHitted.color = color_mapping[new_pb];
		SetHitTextWithTime(timeHit);
		return;
	}

	// If its higher than your current pb
	// (and lower than normal, since other checks failed)
	// Then you can do better!
	if (timeHit > current_pb)
	{
		pBallHitted.color = color_mapping[better_than_normal];
		SetHitTextWithTime(timeHit);
		return;
	}

	// If they end up being equal to either time, just output it normally
	pBallHitted.color = color_mapping[normal];
	SetHitTextWithTime(timeHit);
}

void KickoffTimerPlugin::SetHitTextWithTime(const float time_hit)
{
	std::ostringstream os;

	os << "Ball hitted after " << to_string_with_precision(time_hit, DECIMAL_PRECISION) << " seconds.";
	const auto msg = os.str();
	pBallHitted.text = msg;
}

void KickoffTimerPlugin::SetHitTextWithTimeAndText(const float time_hit, string& message)
{
	std::ostringstream os;
	os << message;
	const auto msg = os.str();
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

	if (nullptr == spawn)
	{
		cvarManager->log("Can't get spawnLocation!.");
		return;
	}

	std::ostringstream os;
	os << spawn->name << " normal Kickoff time: " <<
		to_string_with_precision(spawn->normalTime, DECIMAL_PRECISION) << " seconds";
	auto msg = os.str();
	pDefaultTime.text = msg;

	os.str("");
	os.clear();
	if(spawn->personalBest > 0) os << "Personal Best: " <<
		to_string_with_precision(spawn->personalBest, DECIMAL_PRECISION) << " seconds";
	msg = os.str();
	pBest.text = msg;
}

void KickoffTimerPlugin::Render(CanvasWrapper canvas)
{
	if (!gameWrapper->IsInGame() || popups.empty() || !spawn != 0)
		return;

	const auto screen_size = canvas.GetSize();
	for(auto i = 0; i < popups.size(); i++)
	{
		auto pop = popups.at(i);
		if (pop->startLocation.X < 0)
		{
			pop->startLocation = 
			{
				static_cast<int>(screen_size.X * 0.35),
				static_cast<int>(screen_size.Y * 0.1 + i * 0.035 * screen_size.Y)
			};
		}

		const Vector2 draw_loc = { pop->startLocation.X, pop->startLocation.Y };
		canvas.SetPosition(draw_loc);
		canvas.SetColor(pop->color.R, pop->color.G, pop->color.B, 225);
		canvas.DrawString(pop->text, 3, 3);
	}
}

SpawnLocation* KickoffTimerPlugin::getSpawnLocation() {
	const auto location = gameWrapper->GetLocalCar().GetLocation();

	for (auto& spawn_location : spawnLocations)
	{
		const auto it = &spawn_location;
		if (location.X == it->location.X &&
			location.Y == it->location.Y) {
			return it;
		}
	}
	return nullptr;
}

void KickoffTimerPlugin::savePBFile() {
	ofstream myfile;
	myfile.open(savefile);

	if (!myfile.is_open())
	{
		cvarManager->log("Can't write savefile.");
		myfile.close();
		return;
	}

	for (auto& spawn_location : spawnLocations)
	{
		myfile << spawn_location.personalBest << "\n";
	}

	myfile.close();
}
void KickoffTimerPlugin::loadPBFile() {
	ifstream myfile;
	myfile.open(savefile);
	if (!myfile.good())
	{
		cvarManager->log("Can't read savefile.");
		return;
	}
	
	char buffer[8];

	for (auto& spawn_location : spawnLocations)
	{
		myfile.getline(buffer, 8);

		// There should be the same number of lines in the file as spawnLocations
		if (myfile.eof())
		{
			cvarManager->log("End of savefile reached too early!");
			return;
		}

		const auto f = static_cast<float>(atof(buffer));
		spawn_location.personalBest = f;
	}
}

void KickoffTimerPlugin::updateDecimalValue()
{
	DECIMAL_PRECISION = cvarManager->getCvar("kickofftimer_decimalPlaces").getIntValue();
}
