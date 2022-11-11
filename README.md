# lyrat chatbot


## Usage

Prepare the audio board:

- Connect speakers or headphones to the board. 

Configure the example:

- Select compatible audio board in `menuconfig` > `Audio HAL`
- Get the Google Cloud API Key: https://cloud.google.com/docs/authentication/api-keys 
- Enter API Key, Wi-Fi `ssid` and `password` in `menuconfig` > `Example Configuration`.

Note: If this is the first time you are using above Google API, you need to enable each one of the tree APIs by vising the Google API console.

Load and run the example:

 - Wait for Wi-Fi network connection.
 - Press [Rec] button, and wait for **Red** LED blinking or `GOOGLE_SR: Start speaking now` yellow line in terminal.
 - Speak something in Chinese. If you do not know Chinese then use "Google Translate" to translate some text into Chinese and speak it for you.
 - After finish, release the [Rec] button. Wait a second or two for Google to receive and process the message and then the board to play it back.
- To stop the pipeline press [Mode] button on the audio board.
