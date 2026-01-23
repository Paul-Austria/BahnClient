using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using ModellClientLib.Controllers;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
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
                // Use the same broker IP as your background worker
                .WithTcpServer("192.168.31.123", 1883)
                .WithClientId("WebClient_Listener_" + Guid.NewGuid())
                .WithCleanSession()
                .Build();

            // 2. Handle Incoming Messages
            _mqttClient.ApplicationMessageReceivedAsync += async e =>
            {
                string topic = e.ApplicationMessage.Topic;
                string payload = Encoding.UTF8.GetString(e.ApplicationMessage.Payload);

                if (topic == "trains/command")
                {
                    try
                    {
                        var incomingLoco = JsonSerializer.Deserialize<Locomotive>(payload);
                        if (incomingLoco != null)
                        {
                            // This updates the local cache/DB and triggers the UI Event
                            LocomotiveController.UpdateFromExternal(incomingLoco);
                            _logger.LogInformation($"Web UI Updated for Loco {incomingLoco.ID}");
                        }
                    }
                    catch (Exception ex)
                    {
                        _logger.LogError($"Error parsing MQTT for UI: {ex.Message}");
                    }
                }
            };

            // 3. Connect and Subscribe
            await _mqttClient.ConnectAsync(options, stoppingToken);

            var subOptions = factory.CreateSubscribeOptionsBuilder()
                .WithTopicFilter(f => f.WithTopic("trains/command"))
                .Build();

            await _mqttClient.SubscribeAsync(subOptions, stoppingToken);

            // Keep alive
            while (!stoppingToken.IsCancellationRequested)
            {
                // Optional: Reconnect logic if disconnected
                if (!_mqttClient.IsConnected)
                {
                    await _mqttClient.ConnectAsync(options, stoppingToken);
                }
                await Task.Delay(5000, stoppingToken);
            }
        }
    }
}