using BackgroundMqttWorker.Models;
using ModellClientLib.Controllers;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
using MQTTnet;
using MQTTnet.Client;
using System;
using System.Linq;
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
        static string clientId = "Worker_" + Guid.NewGuid().ToString();

        static async Task Main(string[] args)
        {
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
                    // 1. Initial Data Request (Existing)
                    if (topic == "host/getdata")
                    {
                        Console.WriteLine($"[DATA] Request for: {payload}");
                        await ProcessGetDataRequestAsync(payload);
                    }
                    // 2. Train Control (Existing)
                    else if (topic == "trains/command")
                    {
                        Console.WriteLine($"[CMD] Train Update");
                        ProcessTrainCommand(payload);
                    }
                    // 3. NEW: ESP32 Configuration Request
                    else if (topic == "layout/config/request")
                    {
                        Console.WriteLine($"[CONFIG] Request from: {payload}");
                        await ProcessConfigRequestAsync(client, payload);
                    }
                    else if (topic.EndsWith("/status/heartbeat"))
                    {
                        string controllerName = topic.Replace("/status/heartbeat", "");
                        await ModuleController.UpdateHeartbeatAsync(controllerName);
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error: {ex.Message}");
                }
            };

            await client.ConnectAsync(options);

            var mqttSubscribeOptions = factory.CreateSubscribeOptionsBuilder()
                .WithTopicFilter(f => f.WithTopic("host/getdata"))
                .WithTopicFilter(f => f.WithTopic("trains/command"))
                .WithTopicFilter(f => f.WithTopic("layout/config/request"))
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

        static void ProcessTrainCommand(string payload)
        {
            try
            {
                var incomingLoco = JsonSerializer.Deserialize<Locomotive>(payload);
                if (incomingLoco != null)
                {
                    LocomotiveController.UpdateFromExternal(incomingLoco);
                }
            }
            catch (Exception ex) { Console.WriteLine(ex.Message); }
        }

        static async Task ProcessConfigRequestAsync(IMqttClient client, string payload)
        {
            try
            {
                // Expecting JSON: { "controller_topic": "esp32_main" }
                using var doc = JsonDocument.Parse(payload);
                if (!doc.RootElement.TryGetProperty("controller_topic", out var topicElement)) return;

                string controllerTopic = topicElement.GetString();
                if (string.IsNullOrEmpty(controllerTopic)) return;

                // 1. Get Blocks assigned to this controller
                var blocks = BlockController.GetBlocks()
                    .Where(b => b.ControllerTopic == controllerTopic)
                    .ToList();

                // 2. Send Config for each block
                foreach (var block in blocks)
                {
                    var configPayload = new
                    {
                        id = block.ID,
                        type = block.Type.ToString(),
                        pinA = block.PinA,
                        pinB = block.PinB
                    };

                    string json = JsonSerializer.Serialize(configPayload);

                    var message = new MqttApplicationMessageBuilder()
                        .WithTopic($"{controllerTopic}/config/add_block")
                        .WithPayload(json)
                        .Build();

                    await client.PublishAsync(message);
                    Console.WriteLine($" -> Sent Config for Block {block.ID} to {controllerTopic}");

                    // Small delay to prevent flooding the ESP32 buffer
                    await Task.Delay(50);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error sending config: {ex.Message}");
            }
        }
    }
}