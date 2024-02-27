using ModellClientLib.Context;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace ModellClientLib.Controllers
{
    public static class ServoController
    {
        public static async Task TestServoAsync(Servo servo)
        {
            await MqttClientSingleton.Instance.Transmit(servo.topic, JsonSerializer.Serialize(servo)).ConfigureAwait(false);
        }

        public static void AddServ(Servo servo)
        {
            var dbContext = new ModellDBContext();
            dbContext.servos.Add(servo);
            dbContext.SaveChanges();
        }

        public static IEnumerable<Servo> GetServos() {
            var dbContext = new ModellDBContext();

            return dbContext.servos as IEnumerable<Servo>;
        }

        public static Servo GetByID(int id)
        {
            var dbContext = new ModellDBContext();
            var sw = dbContext.servos.Find(id);
            return sw;
        }

        public static async Task UpdateServoAsync(Servo servo)
        {
            var dbContext = new ModellDBContext();
            dbContext.servos.Update(servo);
            await MqttClientSingleton.Instance.Transmit(servo.topic, JsonSerializer.Serialize(servo)).ConfigureAwait(false);
            dbContext.SaveChanges();
        }


        public static async Task SendCurrentStateByTopicAsync(string topic)
        {
            var dbContext = new ModellDBContext();
            var servos = dbContext.servos.Where(s => s.topic.Contains(topic)).ToList();
            foreach (var servo in servos)
            {
                await MqttClientSingleton.Instance.Transmit(servo.topic, JsonSerializer.Serialize(servo)).ConfigureAwait(false);

            }
        }
        public static void Delete(int id)
        {
            var dbContext = new ModellDBContext();
            var sw = dbContext.servos.Find(id);
            dbContext.servos.Remove(sw);
            dbContext.SaveChanges();
        }
    }
}
