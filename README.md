# KickoffPractice
Kickoff Practice is a BakkesMod plugin for RocketLeague that helps you train your kickoffs by spawning a bot in freeplay to face you on kickoffs.
Using this plugin you can record a kickoff sequence then go in freeplay and spawn a bot to face you on kickoff, the bot will then use the pre-recorded sequence and thus copy your movements.

## How to use the plugin ?
In order to be able to install and use this plugin in RocketLeague you have to first install BakkesMod (PC only).
For more information on how to use or install this plugin go to : https://bakkesplugins.com/plugins/view/328.

## How it works
The plugin works by recording at every game tick the position and velocity of your car (when you're in record mode) and storing them in a file.
Then when you want to practice,the plugin will spawn a bot in freeplay (and teleport you, limit your boost, set up a countdown etc.) and will then, at every tick, set the position and velocity of the bot to those pre-recorded.
That's the core of the plugin, the rest is just UI and little features.

## The reason I published the code
I'm publishing this code because there is still work to do on this plugin but I currently don't have time to do it myself. 
I'm hoping that the community can use what I did to create the ultimate kickoff practice tool.
The things left to do are :
  - change the way the plugin records a kickoff, from recording "states of the game" (position velocity etc.) to recording inputs and then playing them back as the bot.
  The reason this is not implemented already is that it seems to cause compatibility issues : inputs recorded on one computer would not play the same on a different one.
  The problem with the current approach is that the bot will not respect physics when colliding with the ball (as he's position and velocity are pre recorded and not calculated in real time) which is a major problem for a tool aiming to be as close as reality as possible.
  
  - code a way to record a kickoff from a replay. That way you could theoretically face anyone's kickoff (for example the kickoff from a pro player recorded from an RLCS replay file).
  
  - sometimes the player will pick up a small boost pad while being teleported for the kickoff. Fixes could be to find a way to forbid the player to pick up boost while teleporting or to find a way to "refill" boost pads.
  - code a better UI : add morre options/make the current ones less ugly ...

Finally I hope the code is easy enough to understand on your own (english is not my main language so some names in the code may be a mix of french and english 😅). 
As I said I don't have enough time to work on this project anymore so I don't have time to make good documentation. 
