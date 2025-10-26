---
title: HTTP Web API
layout: home
nav_order: 1
parent: Remote Control
---

# HTTP Web API

It's up to you if you want the HTTP Web API of C-Play to be enabled or not. Just change the configuration file in "*data/http-server-conf.json*" to enable/disable the service at runtime. Here you can also modify which port it should utilize.

All api commands are in form of HTTP Post, regardless of what it does, for simplicity.

Below are the various valid endpoints that C-Play responds to. If no specific format is mentioned, it is pure text, in and out.

To easier understand these commands, feel free to utilize a sample Medialon Manager 7 project, that can be found [here](https://github.com/c-toolbox/C-Play/tree/master/help/http_server).

### General ones.

    Endpoint: /status
    Purpose:  To check connection.
    Returns:  "OK"

    Endpoint: /quit
    Purpose:  Ask C-play to quit/exit.
    Returns:  "Quitting C-Play"

    Endpoint: /media_title
    Purpose:  To retrieve the media title of current media.
    Returns:  The title as text/string

    Endpoint: /position
    Purpose:  Which position in time the current media has.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string. 
               Or supply "set=" to actually set a time position.
    Returns:  The time (in seconds) or as formatted time string.

    Endpoint: /seek
    Purpose:  Seek relative time, backward or forward
    Params:   Required to supply "time=value", where value is in seconds. 
              A negative integer is for backward seek and positive integer for forward seek. 
              Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string.
    Returns:  The time (in seconds) or as formatted time string.

    Endpoint: /remaining
    Purpose:  Which remaining time the current media has.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string.
    Returns:  The time (in seconds) or as formatted time string.

    Endpoint: /duration
    Purpose:  Which total duration time the current media has.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string.
    Returns:  The time (in seconds) or as formatted time string.
 
    Endpoint: /play
    Purpose:  Ask C-play to play current media.
    Returns:  "Play"
    
    Endpoint: /pause
    Purpose:  Ask C-play to pause current media.
    Returns:  "Pause"

    Endpoint: /stop or /rewind
    Purpose:  Ask C-play to stop/rewind current media to start position.
    Returns:  "Stop/Rewind"

    Endpoint: /auto_play
    Purpose:  Enable or disables the use of auto play in the playlist.
    Params:   Optional to supply a "on=1" or "on=0" to enable or disable the auto play feature.
    Returns:  When "on=" is added, a message indiciating success or an error. 
              Otherwise returns 0 if auto play is disabled and 1 if it's enabled.

    Endpoint: /speed
    Purpose:  Which playback speed the media player has.
    Params:   Optional to supply "factor=value", to set playback speed to specify value. 
              The value should be between 0.01-100.
    Returns:  The playback speed.

### Handle volume level and image visibility
    
    Endpoint: /volume
    Purpose:  To handle audio volume.
    Params:   Optional to supply "level=" and a value between 0 and 100.
    Returns:  The volume level (0 to 100)

    Endpoint: /fade_duration
    Purpose:  Time value of how long a fade takes in C-Play.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string.
    Returns:  The time (in seconds) or as formatted time string.

    Endpoint: /fade_volume_down
    Purpose:  Ask C-play to lower volume to zero during a certain "fade" time period.
    Returns:  "Fading volume down"

    Endpoint: /fade_volume_up
    Purpose:  Increase volume to previous chosen level during a certain "fade" time period.
    Returns:  "Fading volume up"

    Endpoint: /fade_image_down
    Purpose:  Decrease the visibility of the video/image to 0% during a certain "fade" time period.
    Returns:  "Fading image down"

    Endpoint: /fade_image_up
    Purpose:  Increase the visibility of the image/video to 100% during a certain "fade" time period.
    Returns:  "Fading image up"

    Endpoint: /sync_image_volume_fade
    Purpose:  Increase the image/video view to 100% during a certain "fade" time period.
    Params:   Either supply "value=1" or "value=0" to set if sync between fade is enabled or not
    Returns:  0 or 1 depending on if sync is disabled or enabled. 
              Returns "Setting syncImageVolumeFade to *" when the value parameter is supplied.

    Endpoint: /visibility
    Purpose:  Return the visibility of the image/video.
    Returns:  A value between 0 and 100

## Varios media modes

    Endpoint: /stereo_mode
    Purpose:  Return the current stereoscopic mode in C-play.
    Returns:  A value between 0 and 3

    // 0 = 2D (mono)
    // 1 = 3D (side-by-side)
    // 2 = 3D (top-bottom)
    // 3 = 3D (top-bottom-flip)

    Endpoint: /grid_mode
    Purpose:  Return the current set grid mode in C-play.
    Returns:  A value between 0 and 4

    // 0 = Pre-split
    // 1 = Plane
    // 2 = Dome
    // 3 = Sphere EQR
    // 4 = Sphere EAC

    Endpoint: /loop_mode
    Purpose:  Return the current loop/eof (end of file) mode in C-play.
    Returns:  A value between 0 and 2

    // 0 = Pause
    // 1 = Continue to next
    // 2 = Loop

    Endpoint: /view_mode
    Purpose:  Return or set the "view mode" in C-play, which means to force 2D or let content decide.
    Params:   Optional to set it, using "mode=0" or "mode=1".
    Returns:  A value between 0 and 1

    //0 = Auto 2D/3D switch
    //1 = Force 2D for all

## Handle background and foreground image remotely

    Endpoint: /background_image
    Purpose:  Enable or disables the use of background.
    Params:   Optional to supply a "on=1" or "on=0" to enable or disable the background image.
    Returns:  When "on=" is added, a message indiciating success or an error. 
              Otherwise return 0 if background is disabled and 1 if enabled.

    Endpoint: /background_image_stereo_mode
    Purpose:  Return the stereoscopic mode for the background image.
    Returns:  A value between 0 and 3 (see /stereo_mode enpoint for logic)

    Endpoint: /background_image_grid_mode
    Purpose:  Return the grid mode for the background image.
    Returns:  A value between 0 and 4 (see /grid_mode enpoint for logic)

    Endpoint: /foreground_image
    Purpose:  Enable or disables the use of foreground.
    Params:   Optional to supply a "on=1" or "on=0" to enable or disable the foreground image.
    Returns:  When "on=" is added, a message indiciating success or an error. 
              Otherwise returns 0 if foreground is disabled and 1 if enabled.

    Endpoint: /foreground_image_stereo_mode
    Purpose:  Return the stereoscopic mode for the foreground image.
    Returns:  A value between 0 and 3 (see /stereo_mode enpoint for logic)

    Endpoint: /foreground_image_grid_mode
    Purpose:  Return the grid mode for the foreground image.
    Returns:  A value between 0 and 4 (see /grid_mode enpoint for logic)

## Handle lists remotely

    Endpoint: /playlist
    Purpose:  Retrieve a formatted string of the playlist in C-play.
    Params:   Optional to "charsPerItem=33", or another value to limit the characters in the name.
    Returns:  A formatted text string that can be handled as a list.

    Endpoint: /playing_in_playlist
    Purpose:  Return the index of the current loaded item in the playlist.
    Returns:  A value between 0 and number of items. -1 if nothing is selected.

    Endpoint: /load_from_playlist
    Purpose:  To load a specific item in the playlist.
    Params:   Mandatory to supply "index=", and a value of the index you want to load.
    Returns:  A message to indicate success or an error.

    Endpoint: /audiotracks
    Purpose:  Retrieve a formatted string of all audio tracks current loaded media has.
    Params:   Optional to "charsPerItem=33", or another value to limit the characters in the name. 
              Also optionally to supply "removeLoadedFilePrefix=1" to remove the matching characters 
              from start between video and audio, to shorten the string further.
    Returns:  A formatted text string that can be handled as a list.

    Endpoint: /playing_in_audiotracks
    Purpose:  Return the index of the current loaded audio track.
    Returns:  A value between 0 and number of tracks. -1 if nothing is selected.

    Endpoint: /load_from_audiotracks
    Purpose:  To load a new audio track in the list of audio tracks.
    Params:   Mandatory to supply "index=", and a value of the index you want to load.
    Returns:  A message to indicate success or an error.

    Endpoint: /sections
    Purpose:  Retrieve a formatted string of the sections the current loaded media has.
    Params:   Optional to "charsPerItem=33", or another value to limit the characters in the name.
    Returns:  A formatted text string that can be handled as a list.

    Endpoint: /playing_in_sections
    Purpose:  Return the index of the current loaded section in the section list.
    Returns:  A value between 0 and number of tracks. -1 if nothing is selected.

    Endpoint: /load_from_sections
    Purpose:  To load a specific section in the section list.
    Params:   Mandatory to supply "index=", and a value of the index you want to load.
    Returns:  A message to indicate success or an error.

    Endpoint: /section_start_time
    Purpose:  Time value of when current playing section starts.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string.
    Returns:  The time (in seconds) or as formatted time string.

    Endpoint: /section_end_time
    Purpose:  Time value of when current playing section ends.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similiar to get a format time string.
    Returns:  The time (in seconds) or as formatted time string.

    Endpoint: /section_end_mode
    Purpose:  The mode which handle what happend when the current loaded section ends.
    Returns:  A value between 0 and 4 (see meaning below)

    // 0 = Pause
    // 1 = Fade out (then pause)
    // 2 = Continue
    // 3 = Next
    // 4 = Loop

    Endpoint: /slides
    Purpose:  Retrieve a formatted string of the slides in the loaded C-play presentation.
    Params:   Optional to "charsPerItem=33", or another value to limit the characters in the name.
    Returns:  A formatted text string that can be handled as a list.

    Endpoint: /playing_in_slides
    Purpose:  Return the index of the current loaded item in the slides.
    Returns:  A value between 0 and number of items. -1 if nothing is selected, which indicates master slide.

    Endpoint: /select_from_slides
    Purpose:  To select a specific slide in the slides list (which automatically makes it pre-loads all layers).
    Params:   Mandatory to supply "index=", and a value of the index you want to load.
    Returns:  A message to indicate success or an error.

    Endpoint: /load_from_slides
    Purpose:  To load a specific slide in the slides list.
    Params:   Mandatory to supply "index=", and a value of the index you want to load.
    Returns:  A message to indicate success or an error.

    Endpoint: /layers
    Purpose:  Retrieve a formatted string of the layers in the specific slide.
    Params:   Either supply parameter "slide_name=" with the slide name, for instance "Master".
              Or supply parameter "slide_idx=" with the slide index, -1 for master slide.
              Optional to "charsPerItem=33", or another value to limit the characters in the name.
    Returns:  A formatted text string that can be handled as a list.

## Handle layer parameters
    
    Overall for all layer controls.
    Supply parameter "layer_title=" with the title of the layer
    Or supply parameter "layer_idx" with the index of the layer
    Optional parameter is "slide_name=" with the slide name.
    Or supply parameter "slide_idx=" with the slide index.
    If no slide name or slide idx is added, we assume the master slide.

    Endpoint: /layer_volume
    Purpose:  To handle layer audio volume.
    Params:   Required: see "Overall" above
              Optional to supply "level=" and a value between 0 and 100.
    Returns:  The volume level (0 to 100)

    Endpoint: /layer_visibility
    Purpose:  To handle layer visibility/transparency.
    Params:   Required: see "Overall" above
              Optional to supply "value=" and a value between 0 and 100.
    Returns:  The visibility value (0 to 100)

    Endpoint: /layer_plane
    Purpose:  To handle layer parameters.
    Params:   Required: see "Overall" above
              Optional to supply one or multiple of below params:
              "azimuth", "elevation", "roll", "distance",
              "horizontal", "vertical".
              Send the param with an empty value to just receive current value.
    Returns:  The value of the plane parameter, 
              or list of values (pne per line) if multiple parameters where used.

## Spin the grid

    Endpoint: /orientation_reset
    Purpose:  Reset the orientation of the grid/mapping to default value.
    Returns:  A success message

    Endpoint: /surface_transition
    Purpose:  Launch a transition to a secondary set of values, configured in the settings.
    Params:   Optional to supply "format=hh:mm:ss/zz", or similar, 
              to get a format time string on how long the transition runs.
    Returns:  The transition time (in seconds) or as formatted time string.

    The remaining spin endpoints below follow this scheme:

    Purpose:  Enable or disable the spin controls.
    Params:   Mandatory to supply a "on=1" or "on=0" to enable or disable this spin.
    Returns:  A message indicating success or an error.

    Endpoints that follow these schemes:

    Endpoint: /spin_pitch_up
    Endpoint: /spin_pitch_down
    Endpoint: /spin_yaw_left
    Endpoint: /spin_yaw_right
    Endpoint: /spin_roll_ccw
    Endpoint: /spin_roll_cw

## JSON calls

    Endpoint: /playfile_json
    Purpose:  To return a JSON structure of the current media state, stored as cplayfile
    Returns:  A json structure

    Endpoint: /playlist_json
    Purpose:  To return a JSON structure of the playlist.
    Returns:  A json structure