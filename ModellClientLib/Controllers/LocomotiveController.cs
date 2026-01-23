using Microsoft.EntityFrameworkCore;
using ModellClientLib.Context;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
using System.Text.Json;

namespace ModellClientLib.Controllers
{
    public static class LocomotiveController
    {
        public static event Action? OnDataChanged;

        private static void NotifyDataChanged()
        {
            OnDataChanged?.Invoke();
        }

        public static void AddLocomotive(Locomotive loco)
        {
            using var dbContext = new ModellDBContext();
            dbContext.Locomotives.Add(loco);
            dbContext.SaveChanges();

            NotifyDataChanged();
        }

        public static void Delete(int id)
        {
            using var dbContext = new ModellDBContext();
            var loco = dbContext.Locomotives.Find(id);
            if (loco != null)
            {
                dbContext.Locomotives.Remove(loco);
                dbContext.SaveChanges();

                NotifyDataChanged();
            }
        }
        public static async Task UpdateLocoStateAsync(Locomotive loco)
        {
            using var dbContext = new ModellDBContext();
            dbContext.Locomotives.Update(loco);
            dbContext.SaveChanges();

            NotifyDataChanged();

            string payload = JsonSerializer.Serialize(loco);
            await MqttClientSingleton.Instance.Transmit(loco.Topic+"/"+loco.ID, payload).ConfigureAwait(false);
        }

        public static IEnumerable<Locomotive> GetLocomotives()
        {
            using var dbContext = new ModellDBContext();
            // Load all locos with their functions
            return dbContext.Locomotives
                            .Include(l => l.Functions)
                            .ToList();
        }

        public static Locomotive GetByID(int id)
        {
            using var dbContext = new ModellDBContext();
            return dbContext.Locomotives
                            .Include(l => l.Functions)
                            .FirstOrDefault(l => l.ID == id);
        }

        /// <summary>
        /// Emergency Stop: Sets speed to 0 for a specific loco
        /// </summary>
        public static async Task EmergencyStopAsync(int locoId)
        {
            var loco = GetByID(locoId);
            if (loco != null)
            {
                loco.Speed = 0;
                await UpdateLocoStateAsync(loco);
            }
        }

        public static async Task UpdateFunctionStateAsync(LocoFunction func)
        {
            using var dbContext = new ModellDBContext();
            dbContext.Entry(func).State = EntityState.Modified;
            dbContext.SaveChanges();

            NotifyDataChanged();

            var parentLoco = dbContext.Locomotives.Find(func.LocomotiveID);
            if (parentLoco != null)
            {
                string payload = JsonSerializer.Serialize(parentLoco);
                await MqttClientSingleton.Instance.Transmit(parentLoco.Topic, payload).ConfigureAwait(false);
            }
        }


        public static async Task SendCurrentStateByTopicAsync(string topicFilter)
        {
            using var dbContext = new ModellDBContext();
            var locos = dbContext.Locomotives
                                 .Include(l => l.Functions)
                                 .AsEnumerable() // Switch to client-side eval for complex string matching if needed
                                 .Where(l => l.Topic.Contains(topicFilter) || topicFilter.Contains("trains"));

            foreach (var loco in locos)
            {
                string payload = JsonSerializer.Serialize(loco);
                await MqttClientSingleton.Instance.Transmit(loco.Topic, payload).ConfigureAwait(false);
            }
        }

        public static void UpdateFromExternal(Locomotive incomingData)
        {
            using var dbContext = new ModellDBContext();

            var existingLoco = dbContext.Locomotives
                                        .Include(l => l.Functions)
                                        .FirstOrDefault(l => l.ID == incomingData.ID);

            if (existingLoco != null)
            {
                existingLoco.Speed = incomingData.Speed;
                existingLoco.Direction = incomingData.Direction;

                if (incomingData.Functions != null)
                {
                    foreach (var inFunc in incomingData.Functions)
                    {
                        var existingFunc = existingLoco.Functions.FirstOrDefault(f => f.FIndex == inFunc.FIndex);
                        if (existingFunc != null)
                        {
                            existingFunc.IsActive = inFunc.IsActive;
                        }
                    }
                }

                dbContext.SaveChanges();

                NotifyDataChanged();

            }
        }

    }
}