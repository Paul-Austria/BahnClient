using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using ModellClientLib.Controllers;
using ModellClientLib.Models;
using MQTTnet;
using MQTTnet.Client;
using System;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace BahnClient.Services
{
    public class WebsiteMqttListener : BackgroundService
    {
        private readonly ILogger<WebsiteMqttListener> _logger;
        private IMqttClient _mqttClient;

        public WebsiteMqttListener(ILogger<WebsiteMqttListener> logger)
        {
            _logger = logger;
        }

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
            // 1. Setup MQTT Client
            var factory = new MqttFactory();
            _mqttClient = factory.CreateMqttClient();

            var options = new MqttClientOptionsBuilder()
                .WithTcpServer("192.168.31.123", 1883)
                .WithClientId("WebClient_Listener_" + Guid.NewGuid())
                .WithCleanSession()
                .Build();

            // 2. Handle Incoming Messages
            _mqttClient.ApplicationMessageReceivedAsync += async e =>
            {
                string topic = e.ApplicationMessage.Topic;
                string payload = Encoding.UTF8.GetString(e.ApplicationMessage.Payload);

                // TRAIN UPDATES
                if (topic == "trains/command")
                {
                    try
                    {
                        var incomingLoco = JsonSerializer.Deserialize<Locomotive>(payload);
                        if (incomingLoco != null)
                        {
                            LocomotiveController.UpdateFromExternal(incomingLoco);
                            _logger.LogInformation($"Web UI Updated for Loco {incomingLoco.ID}");
                        }
                    }
                    catch (Exception ex)
                    {
                        _logger.LogError($"Error parsing Train MQTT: {ex.Message}");
                    }
                }
                // BLOCK UPDATES (Wildcard match: ends with /control/block_update)
                else if (topic.EndsWith("/control/block_update"))
                {
                    try
                    {
                        // Expected Payload: 
                        // { "id": 1, "occupied": true, "magnet_count": 5, "direction": "Forward" }
                        // OR
                        // { "id": 1, "cmd": "reset" } - handled by controller usually, but listening here ensures UI sync

                        using var doc = JsonDocument.Parse(payload);
                        var root = doc.RootElement;

                        if (root.TryGetProperty("id", out var idProp))
                        {
                            int blockId = idProp.GetInt32();

                            // Check if this is a full state update from ESP32
                            if (root.TryGetProperty("occupied", out var occProp))
                            {
                                bool isOccupied = occProp.GetBoolean();
                                int count = root.TryGetProperty("magnet_count", out var countProp) ? countProp.GetInt32() : 0;
                                string dir = root.TryGetProperty("direction", out var dirProp) ? dirProp.GetString() : "None";

                                await BlockController.UpdateStateAsync(blockId, isOccupied, dir, count);
                                _logger.LogInformation($"Web UI Updated Block {blockId}");
                            }
                            else if (root.TryGetProperty("cmd", out var cmdProp))
                            {
                                string cmd = cmdProp.GetString();
                                if (cmd == "reset") await BlockController.ResetBlockAsync(blockId);
                                if (cmd == "swap_direction") await BlockController.FlipDirectionAsync(blockId);
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        _logger.LogError($"Error parsing Block MQTT: {ex.Message}");
                    }
                }
            };

            // 3. Connect and Subscribe
            await _mqttClient.ConnectAsync(options, stoppingToken);

            var subOptions = factory.CreateSubscribeOptionsBuilder()
                .WithTopicFilter(f => f.WithTopic("trains/command"))
                .WithTopicFilter(f => f.WithTopic("+/control/block_update"))
                .Build();

            await _mqttClient.SubscribeAsync(subOptions, stoppingToken);

            while (!stoppingToken.IsCancellationRequested)
            {
                if (!_mqttClient.IsConnected)
                {
                    try
                    {
                        await _mqttClient.ConnectAsync(options, stoppingToken);
                        await _mqttClient.SubscribeAsync(subOptions, stoppingToken);
                    }
                    catch {  }
                }
                await Task.Delay(5000, stoppingToken);
            }
        }
    }
}