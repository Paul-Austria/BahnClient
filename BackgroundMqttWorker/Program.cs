

using BackgroundMqttWorker.Models;
using ModellClientLib.Controllers;
using ModellClientLib.Mqtt;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Server;
using System;
using System.Text;
using System.Text.Json;

namespace HelloWorld
{
    class ModellTestClient
    {
        static string broker = "192.168.1.180";
        static int port = 1883;
        static string clientId = Guid.NewGuid().ToString();

        static async Task Main(string[] args)
        {
            //Connect LibCLient
            MqttClientSingleton.Instance.Connect();
            var factory = new MqttFactory();

            // Create a MQTT client instance
            var client = factory.CreateMqttClient();

            // Create MQTT client options
            var options = new MqttClientOptionsBuilder()
                .WithTcpServer(broker, port) // MQTT broker address and port
                .WithClientId(clientId)
                .WithCleanSession()
                .Build();


            client.ApplicationMessageReceivedAsync += async e =>
            {
                Console.WriteLine("Received application message.");
                if (e.ApplicationMessage == null) return;
                string topic = e.ApplicationMessage.Topic;
                if (e.ApplicationMessage.Payload != null)
                {
                    string payload = Encoding.ASCII.GetString(e.ApplicationMessage.Payload);

                    if (topic == "host/getdata")
                    {
                        await ProcessGetDataRequestAsync(payload);
                    }
                }

            };

            var connectResult = await client.ConnectAsync(options).ConfigureAwait(false);


            var mqttSubscribeOptions = factory.CreateSubscribeOptionsBuilder()
               .WithTopicFilter(
                   f =>
                   {
                       f.WithTopic("host/getdata");
                   })
            .Build();

            await client.SubscribeAsync(mqttSubscribeOptions, CancellationToken.None);


            //Run forever
            while (true);
        }


        static async Task ProcessGetDataRequestAsync(string payload)
        {
            if (payload == null) return;
            try
            {
                GetDataModel dt = JsonSerializer.Deserialize<GetDataModel>(payload);

                await ServoController.SendCurrentStateByTopicAsync(dt.Topic);
                await SwitchController.SendCurrentStateByTopicAsync(dt.Topic);
            }catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }
    }
}