
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Protocol;
using MQTTnet.Server;
using System;
using System.Runtime.CompilerServices;

namespace ModellClientLib.Mqtt
{
    public class MqttClientSingleton
    {
        string broker = "192.168.1.180";
        int port = 1883;
        string clientId = Guid.NewGuid().ToString();
        private static MqttClientSingleton _instance;
        public IMqttClient client { get; private set; }
        private MqttClientSingleton()
        {
        }




    public async Task Transmit(string topic, string msg)
    {
        if (client != null && client.IsConnected)
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
            Connect();
            // Handle the case where the client is not connected
            Console.WriteLine("MQTT client is not connected. Trying to Connect");
        }
    }

        public async void Connect()
        {

            if(client == null || !client.IsConnected)
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

                var connectResult = await client.ConnectAsync(options).ConfigureAwait(false);
            }

        }


        public static MqttClientSingleton Instance
        {
            get
            {
                // Create a new instance if it doesn't exist
                if (_instance == null)
                {
                    _instance = new MqttClientSingleton();
                }
                 
                 return _instance;
            }
        }
        
    }
}
