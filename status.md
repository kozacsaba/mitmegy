client server relationship is established, communication does work. i was able to rescan plugins, and list them

client command parsing is weak and fragile. i was not able to add a plugin, because the id and idx kept bleeding into each other, and when i tryed a value that was supposed to be valid for both of them, the registry created a null instance

i need to test and debug every single command and make sure they do what i think they should do before moving to gui

TESTS:
[ ] req_query
[x] - plugin
[x] - chain
[x] - device
[ ] - path
[x] req_add_plugin
[x] req_bypass_plugin
[ ] req_remove_plugin
[ ] req_move_plugin
[ ] req_open_gui
[ ] req_rescan
[ ] req_add_path
[ ] req_remove_path
[x] req_select_device
[ ] req_save_snapshot
[ ] req_load_snapshot

[x] res_completed
[x] res_error
[x] res_query_plugins
[x] res_query_chain
[x] res_query_devices
[x] res_failed_plugins

QUIRKS:
- controller throws if processor is unavailable (fine, but should be caught)
- processor stalls with no active audio (if no apps are producing audio) - meaning plugin chain tasks are blocked
- ctrl C does not stop the processor
