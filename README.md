# ESP8266 DHT22 Web Server

This project is to program ESP8266 boards to offer a simple HTTP server to read temperature and humidity data of an attached DHT22 sensor.

## The Server
The server code, under [`dht22-web`](dht22-web), is developed for ESP8266 chipset. I used [Arduino IDE](https://www.arduino.cc/en/Main/Software) to develop and upload this code on all my ESP8266 chipsets. 

### API 
The service offers a very simple API which is  specified in the [swagger.yml](api/swagger.yml) file:

#### Reading
**`GET /`** provides the current reading of the sensor. 

Example response:

```
{
    "deltaTime": 0,
    "device": "dht22-03",
    "dht22": {
        "heatIndex": 21.85072,
        "humidity": 61.1,
        "temperature": 22
    },
"stale": 0
}
```
where
* `deltaTime` specifies the time of reading as the number of milliseconds in the past. 
* `stale`, basically a boolean with values of 0 or 1, defines if the reading is considered to be *old* (value being `1`). If the data is *stale*, it means that there has been a problem with reading the sensor. 

#### WiFi Configuration

**`GET /config?ssid=<SSID>&password=<PASSWORD>`**
configures the board with a new SSID and an *optional* password. If you think why not a `POST` method, it is just because of pragmatic reasons. People who are not comfortable with `curl` and such tools, can simply use their browsers to enter the URL. 

## A Note on the WiFi Configuration Process
The main challenge with WiFi-based devices is how to configure the connection. The most common approach is that the device offers its own hotspot to which the user can connect and using an endpoint configure the device. Although, this seems very reasonable, in practice this is still not a very solid solution. So many times I had problems connecting to my boards, or having my laptop refusing to connect to the hotspot or later not being able to connect the board to my WiFi even with the correct password. A quick search shows that these are still common issues. That's why I aimed for a practical approach:

* The board can be pre-programmed with a fall-back WiFi connection. The good thing about it is that you can always make such a hotspot when needed. This is defined by the `def_ssid` and `def_password` constants.

* When the device comes up, it looks for a connection configuration. If there is a user-defined configuration, it tries to connect to the configured network. If the configuration is missing, or the connection cannot be established within 30 seconds, it falls back to the pre-programmed settings. 

## Old ESP8266 Chips
If you have older ESP8266 chips (I have quite a few), make sure that you update the firmware before programming them. Google will help you find the way, but if you have issues, just contact me and I will share with you what I know. 

## License
The code is published under an [MIT license](LICENSE.md). 

## Contributions
Please report issues or feature requests using Github issues. Code contributions can be done using pull requests. 

