# groov-in-c
An easy to self-host Discord music bot written in C

## Hosting

Prerequisites:
 - A Discord bot token: https://discordjs.guide/preparations/setting-up-a-bot-application.html#creating-your-bot
 - A free Heroku account: https://signup.heroku.com/

Note: By default the bot will only run for 22 days a month. Check out the first question in the [FAQ](#faq) if you want it to always be online.

### 1. Add to Heroku

[![Deploy](https://www.herokucdn.com/deploy/button.svg)](https://heroku.com/deploy?template=https://github.com/Among-Suss/groov-in-c/tree/main)

### 2. Deploy App
 - Enter your token and a unique bot name (the name is only for options; it won't affect the bot's display name)

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

## FAQ

**Q**: Why did my bot stopped after a couple of weeks?

**A**: The Heroku free tier only allows 550 hours of total run time per month, which translates to about 23 days. If you want it to be always online, you can increase this number to 1000 hours by [adding your credit card and verifying your account](https://devcenter.heroku.com/articles/account-verification#:~:text=Go%20to%20your%20Account%20Settings,Click%20Add%20Credit%20Card%20.). Since 1000 hours is more than a month, Heroku won't charge you ***as long as you only have one application running***. If you are already using Heroku for other applications, make sure you pay attention to your total hours!

## 

