# groov-in-c
An easy to self-host Discord music bot written in C

## Table of contents


 - [Hosting](#hosting)
 - [Using the bot](#using-the-bot)
 - [FAQ](#faq)
 - [Build Instructions](#build-instructions)


## Hosting

Prerequisites:
 - A Discord bot token: https://discordjs.guide/preparations/setting-up-a-bot-application.html#creating-your-bot/
 - A free Heroku account: https://signup.heroku.com/

Note: By default the bot will only run for 22 days a month. Check out the first question in the [FAQ](#faq) if you want it to always be online.

### 1. Add to Heroku

[![Deploy](https://www.herokucdn.com/deploy/button.svg)](https://heroku.com/deploy?template=https://github.com/Among-Suss/groov-in-c/tree/main)

### 2. Creating and Deploy App
 - Enter your token and a unique bot name (the name is only for options; it won't affect the bot's display name)
 - The BOT_NAME field should NOT contain any spaces. for example: my-groov-in-c-bot

### 3. Activate Bot
1. Go to the app through the Heroku Dashboard.
2. Go to the **Resources Tab**.
3. Under the **Free Dynos** section, click on the pencil icon, turn on the switch (it should be on the right side), and click **Confirm**. 
    * *If it says "no worker", wait a couple minutes then refresh the page.*
4. Your Bot should be online!

### 4. Add to server

1. Go to your **Discord Developer Console** and find your bot application.
2. From the **OAuth2** tab, check *bot* under scopes and the following permissions under **Bot Permissions**.
    - General Permissions
        - View Channels
    - Text Permissions
        - Send Messages
        - Embed Links
        - Attach Files
        - Read Message History
        - Add Reactions
        - Use Slash Commands
     - Voice Permissions
        - Connect
        - Speak
        - Video
        - Use Voice Activity
3. Go to link from the **Scopes** section and add the bot to your server.

## Using the bot

### Configuring the prefix
The default prefix for the bot is "-". The prefix can be changed at runtime with the command:
```
[prefix]prefix newprefix
```
for example:
```
-prefix !
```
This will change the prefix from - to !

This change will only last until the bot gets restarted. To make the change permanent, add a prefix configuration to the bot configuration.

Add the following text to your welcome channel's topic (description), or any other channel that the bot has permission to view.
```
@[bot-name] prefix[myprefix] end
```
Replace [bot-name] with what you put in your BOT_NAME on heroku or environment variable.
for example:
```
@groov-in-c prefix! end
```
This will change the default prefix to ! for the bot named groove-in-c. This configuration is applied when the bot starts or reboots. To apply changes to the bot without rebooting, also use the -prefix command as well.

### List of commands
Commands are written in the following format
[prefix]command [arguments]

The list here assumes the prefix is "-".

```
Play music:                 -p [youtube link or text to search youtube]
  Play next (cut queue):    -pn [youtube link or text]
Pause music:                -pause
Resume after pause:         -play
Skip music:                 -skip
Show queue:                 -queue         (or -queue [page number])
Show current song:          -np
Delete entry from queue:    -r [entry number in queue]
Show youtube description:   -desc
Make the bot leave:         -leave
Clear queue:                -clear
Shuffle queue:              -shuffle
Seek music:                 -seek [hour]:[mins]:[secs]
    Example: -seek 3:20 (goes to 3 minutes 20 seconds in the song)
```

The -p command takes either a youtube link or search query
For example:
```
-p ghibli jazz playlist
-p https://youtu.be/h03hS1qGSLA
-p https://www.youtube.com/watch?v=h03hS1qGSLA
```

These are all valid. Playlists can be queued. If a song inside a playlist is queued, the queue will start from the song's index.

### Bot configuration
Bot server specific configuration can be applied by putting the configuration string in the server's welcome channel topic. The configuration string follows a very specific syntax.
```
@[bot-name] config[value] config[value] ... ... end
```
List of possible configurations:
```
Prefix:
    config      - prefix
    value       - any single character
    example     - @groov-in-c prefix! end

DJ Roles (Permission to control music):
    config      - djroles
    value       - coma separated values of roles enclosed in double quotes
    example     - @groov-in-c djroles"DJ","admin","Music Guy" end
    note        - if djroles is not specified, everyone will be able to play music
```
More configurations may be added in the future.

## FAQ

**Q**: Why did my bot stopped after a couple of weeks?

**A**: With the Heroku free tier, you only get 550 total hours of run time. If you want it to be always online, you can increase this number to 1000 hours by adding your credit card and [verifying your account](https://devcenter.heroku.com/articles/account-verification#:~:text=Go%20to%20your%20Account%20Settings,Click%20Add%20Credit%20Card%20.). Since 1000 hours is more than a month, Heroku won't charge you ***as long as you only have one application running***. If you are already using Heroku for other applications, make sure you pay attention to your total hours!

##

**Q**: Why is the play command *sometimes* failing?

**A**: There's always a very small chance that the command would fail due to how youtube-dl works, but playing the song again should work.

##

**Q**: Why is the play command always failing?

**A**: If the play command fails everytime, try and restart the bot by turning the dyno on and off (see [bot activation](#3-activate-bot)).

##

**Q**: Why does the bot randomly disconnect and reconnect?

**A**: For some reason, the Discord API will randomly disconnect the bot, which can take a moment to reconnect.

## Build Instructions
You can just run this repository through docker if that's your thing. This program only supports linux, but it is possible to get working on Windows using cygwin with some modification (not covered here).

### Environment Variables
| Name | Description | Default |
|------|-------------|---------|
| `TOKEN` | Discord bot token | *required* |
| `BOT_PREFIX` | Bot prefix | `-` |
| `BOT_NAME` | Bot config name | `groov-in-c` |


### Prerequisites

#### Ubuntu and Debian
1. Install all the build and runtime dependencies
```
sudo apt-get install make gcc libssl-dev libsodium-dev libopus-dev libogg-dev ffmpeg python3-pip
```

2. Install youtube-dl with pip since Ubuntu's version is outdated
```
sudo pip install youtube-dl
```

#### Arch Linux
Install all the build and runtime dependencies
```
sudo pacman -Syu
sudo pacman -S base-devel openssl libsodium libogg opus python ffmpeg youtube-dl python-requests
```

### Building
1. Make sure you are in the directory groov-in-c/src/discord-in-c
2. Run make
```
make test2
```

### Running
There are two environment variables that the bot needs
1. TOKEN - Discord bot token generated in the discord developer portal
2. BOT_NAME - the name of the bot. NOT the display name. This name is the identifier for use in configuring server-specific configs

Either set the env vars by using the export command or run with the env vars declared

Make sure to be in the directory groov-in-c/src/discord-in-c
```
TOKEN="mytoken" BOT_NAME="my-groov-in-c" ./test2
```

## 
