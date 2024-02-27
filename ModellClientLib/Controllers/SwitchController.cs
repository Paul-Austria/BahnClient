using ModellClientLib.Context;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Text.Json;

namespace ModellClientLib.Controllers
{
    public static class SwitchController
    {
        public static void AddSwitch(Switch sw)
        {
            var dbContext = new ModellDBContext();
            dbContext.switches.Add(sw);
            dbContext.SaveChanges();
        }
        public static Switch GetByID(int id)
        {
            var dbContext = new ModellDBContext();
            var sw = dbContext.switches.Find(id);
            return sw;
        }
        public static void Delete(int id)
        {
            var dbContext = new ModellDBContext();
            var sw = dbContext.switches.Find(id);
            dbContext.switches.Remove(sw);
            dbContext.SaveChanges();
        }
        public static IEnumerable<Switch> GetSwitches()
        {
            var dbContext = new ModellDBContext();

            return dbContext.switches as IEnumerable<Switch>;
        }

        public static async Task SendCurrentStateByTopicAsync(string topic)
        {
            var dbContext = new ModellDBContext();
            var switches = dbContext.switches.Where(s => s.topic.Contains(topic));
            foreach (var sw in switches)
            {
                await MqttClientSingleton.Instance.Transmit(sw.topic, JsonSerializer.Serialize(sw)).ConfigureAwait(false);

            }
        }
        public static async Task UpdateSWAsync(Switch sw) {
            var dbContext = new ModellDBContext();
            dbContext.Update(sw);
            await MqttClientSingleton.Instance.Transmit(sw.topic, JsonSerializer.Serialize(sw)).ConfigureAwait(false);
            dbContext.SaveChanges();
        }
    }
}
