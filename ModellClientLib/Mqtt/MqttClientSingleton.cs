using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using System;
using System.Text;
using System.Threading.Tasks;

namespace ModellClientLib.Mqtt
{
    public class MqttClientSingleton
    {
        string broker = "192.168.31.123";
        int port = 1883;
        string clientId = Guid.NewGuid().ToString();
        private static MqttClientSingleton _instance;
        public IMqttClient client { get; private set; }

        // 1. ADDED: Event to notify the Blazor page when a message arrives
        public event Action<string, string> OnMessageReceived;

        private MqttClientSingleton()
        {
        }

        public async Task Transmit(string topic, string msg)
        {
            await EnsureConnectedAsync();

            if (client != null && client.IsConnected && !string.IsNullOrEmpty(topic))
            {
                var message = new MqttApplicationMessageBuilder()
                    .WithTopic(topic)
                    .WithPayload(msg)
                    .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
                    .WithRetainFlag(false)
                    .Build();

                await client.PublishAsync(message).ConfigureAwait(false);
            }
            else
            {
                Console.WriteLine("MQTT client is not connected. Message not sent.");
            }
        }

        public async Task SubscribeAsync(string topic)
        {
            await EnsureConnectedAsync();

            if (client != null && client.IsConnected)
            {
                var mqttFactory = new MqttFactory();
                var subscribeOptions = mqttFactory.CreateSubscribeOptionsBuilder()
                    .WithTopicFilter(f => f.WithTopic(topic))
                    .Build();

                await client.SubscribeAsync(subscribeOptions);
                Console.WriteLine($"Subscribed to topic: {topic}");
            }
        }

        public async Task EnsureConnectedAsync()
        {
            if (client == null)
            {
                var factory = new MqttFactory();
                client = factory.CreateMqttClient();

                client.ApplicationMessageReceivedAsync += e =>
                {
                    string receivedTopic = e.ApplicationMessage.Topic;

                    string payloadStr = e.ApplicationMessage.ConvertPayloadToString();

                    OnMessageReceived?.Invoke(receivedTopic, payloadStr);

                    return Task.CompletedTask;
                };
            }

            if (!client.IsConnected)
            {
                var options = new MqttClientOptionsBuilder()
                    .WithTcpServer(broker, port)
                    .WithClientId(clientId)
                    .WithCleanSession()
                    .Build();

                await client.ConnectAsync(options);
            }
        }

        public static MqttClientSingleton Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = new MqttClientSingleton();
                }
                return _instance;
            }
        }

        public async void Connect()
        {

            if (client == null || !client.IsConnected)
            {
                var factory = new MqttFactory();

                // Create a MQTT client instance
                client = factory.CreateMqttClient();

                // Create MQTT client options
                var options = new MqttClientOptionsBuilder()
                    .WithTcpServer(broker, port) // MQTT broker address and port
                    .WithClientId(clientId)
                    .WithCleanSession()
                    .Build();

                var connectResult = await client.ConnectAsync(options);
            }

        }
    }
}