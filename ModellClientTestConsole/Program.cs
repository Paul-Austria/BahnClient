using ModellClientLib.Context;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
using MQTTnet.Protocol;
using MQTTnet;

namespace HelloWorld
{
    class ModellTestClient
    {
        static async Task Main(string[] args)
        {
            MqttClientSingleton.Instance.Connect();
            var testDB = new ModellDBContext();
            foreach(var sw in testDB.switches)
            {
               Console.WriteLine(sw.ToString());
            }

            for (int i = 0; i < 10; i++)
            {
                var message = new MqttApplicationMessageBuilder()
                    .WithTopic("test")
                    .WithPayload($"Hello, MQTT! Message number {i}")
                    .WithQualityOfServiceLevel(MqttQualityOfServiceLevel.AtLeastOnce)
                    .WithRetainFlag()
                    .Build();

                await MqttClientSingleton.Instance.client.PublishAsync(message).ConfigureAwait(false);
                await Task.Delay(1000); // Wait for 1 second
            }

        }
    }
}