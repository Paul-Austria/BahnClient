using BackgroundMqttWorker.Models;
using ModellClientLib.Controllers;
using ModellClientLib.Models; // Ensure you have this for Locomotive model
using ModellClientLib.Mqtt;
using MQTTnet;
using MQTTnet.Client;
using System;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace HelloWorld
{
    class ModellTestClient
    {
        static string broker = "192.168.31.123";
        static int port = 1883;
        static string clientId = Guid.NewGuid().ToString();

        static async Task Main(string[] args)
        {
            // Connect internal LibClient for sending responses
            MqttClientSingleton.Instance.Connect();

            var factory = new MqttFactory();
            var client = factory.CreateMqttClient();

            var options = new MqttClientOptionsBuilder()
                .WithTcpServer(broker, port)
                .WithClientId(clientId)
                .WithCleanSession()
                .Build();

            client.ApplicationMessageReceivedAsync += async e =>
            {
                if (e.ApplicationMessage.Payload == null) return;

                string topic = e.ApplicationMessage.Topic;
                string payload = Encoding.UTF8.GetString(e.ApplicationMessage.Payload);

                try
                {
                    if (topic == "host/getdata")
                    {
                        Console.WriteLine($"Received Data Request: {payload}");
                        await ProcessGetDataRequestAsync(payload);
                    }
                    else if (topic == "trains/command")
                    {
                        Console.WriteLine($"Received Train Command: {payload}");
                        ProcessTrainCommand(payload);
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error processing MQTT message: {ex.Message}");
                }
            };

            await client.ConnectAsync(options).ConfigureAwait(false);

            var mqttSubscribeOptions = factory.CreateSubscribeOptionsBuilder()
                .WithTopicFilter(f => f.WithTopic("host/getdata")) 
                .WithTopicFilter(f => f.WithTopic("trains/command")) 
                .Build();

            await client.SubscribeAsync(mqttSubscribeOptions, CancellationToken.None);

            Console.WriteLine("Background Worker Listening...");

            await Task.Delay(Timeout.Infinite);
        }

        static async Task ProcessGetDataRequestAsync(string payload)
        {
            try
            {
                var dt = JsonSerializer.Deserialize<GetDataModel>(payload);
                if (dt == null) return;

                await ServoController.SendCurrentStateByTopicAsync(dt.Topic);

                await SwitchController.SendCurrentStateByTopicAsync(dt.Topic);

                await LocomotiveController.SendCurrentStateByTopicAsync(dt.Topic);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in GetData: {ex.Message}");
            }
        }

        // Handle "trains/command"
        static void ProcessTrainCommand(string payload)
        {
            try
            {
                var incomingLoco = JsonSerializer.Deserialize<Locomotive>(payload);

                if (incomingLoco != null)
                {
                    LocomotiveController.UpdateFromExternal(incomingLoco);
                    Console.WriteLine($"Updated Loco {incomingLoco.ID} Speed to {incomingLoco.Speed}");
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error in TrainCommand: {ex.Message}");
            }
        }
    }
}