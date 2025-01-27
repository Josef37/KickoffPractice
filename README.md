# KickoffPractice
Kickoff Practice is a BakkesMod plugin for RocketLeague that helps you train your kickoffs by spawning a bot in freeplay to face you on kickoffs.
Using this plugin you can record a kickoff sequence then go in freeplay and spawn a bot to face you on kickoff, the bot will then use the pre-recorded sequence and thus copy your movements.

## How to use the plugin
In order to be able to install and use this plugin in RocketLeague you have to first install BakkesMod (PC only).
For more information on how to use or install this plugin go to : https://bakkesplugins.com/plugins/view/328.

## How it works
The plugin works by recording at every game tick the position and velocity of your car (when you're in record mode) and storing them in a file.
Then when you want to practice,the plugin will spawn a bot in freeplay (and teleport you, limit your boost, set up a countdown etc.) and will then, at every tick, replay the pre-recorded inputs with the bot.
That's the core of the plugin, the rest is just UI and little features.
If you don't know how to code plugins and want to learn please visit https://wiki.bakkesplugins.com/plugin_tutorial/getting_started/ and join the [BakkesMod programming discord](https://discord.gg/s97RgrgkxE).

## The things left to do
  - code a way to record a kickoff from a replay. That way you could theoretically face anyone's kickoff (for example the kickoff from a pro player recorded from an RLCS replay file).
  - code a better UI : add more options/make the current ones less ugly ...
