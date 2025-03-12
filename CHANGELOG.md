# Changelog

## 2.3.0

- Added support for multiple game modes: Soccar, Hoops, Dropshot, and Snowday.
- Fully integrated the Speedflip Trainer, removing the external dependency.
- Fixed an issue where scoring a goal didn't reset the kickoff.

## 2.2.0

- Made file and directory names lower-case.

## 2.1.0

- Fixed accuracy issues: The replayed kickoff now matches the recorded one perfectly.
- Updated the plugin to only load in freeplay.
- Added commands for all UI actions: Select, Repeat, and Reset.
- Fixed crashing issues after a goal is scored.
- Fixed demolition ending training prematurely.
- Fixed sideways travel in the Speedflip Trainer.

## 2.0.0

- Changed recording and replaying states to inputs, preventing the bot from always winning the kickoff.
- Updated the recording to store car body, kickoff position, and control settings.
- Integrated the Speedflip Trainer into the plugin.
- Added the ability to rename and delete recordings.
- Added a mode indicator.
- Fixed pausing not being respected during training.
- Persisted user settings across sessions.
- Added auto-restart and restart functionality when resetting freeplay.
- Added support for replaying recordings.
- Added support for slow-motion playback.
- Added the ability to save the last attempt.
- Fixed bots not being removed after resetting freeplay.
- Fixed the car not falling to the ground on kickoff.
- Fixed boost reset issues after completing a kickoff.
- Fixed accidental boost pickup during training.
