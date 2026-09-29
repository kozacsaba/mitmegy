client server relationship is established, communication does work. i was able to rescan plugins, and list them

client command parsing is weak and fragile. i was not able to add a plugin, because the id and idx kept bleeding into each other, and when i tryed a value that was supposed to be valid for both of them, the registry created a null instance

i need to test and debug every single command and make sure they do what i think they should do before moving to gui
