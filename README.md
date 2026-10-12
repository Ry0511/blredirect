# Borderlands Redirector

Patches the package loading pipeline so that modified base game packages are loaded from the 
modified package file and not from the saved copy stored in the cooked package. This should remove 
the need for Startup File for Mods and part of Editable Base Maps.

# Installation

1. Grab the latest version of the [plugin loader](https://github.com/bl-sdk/pluginloader/releases)
2. Grab the latest version of [Borderlands Redirector](https://github.com/Ry0511/blredirect/releases)
3. Extract the contents of `blredirector-*-x86.zip` into the plugins directory

> `blredirect.log` will be created containing useful information if an error should occur