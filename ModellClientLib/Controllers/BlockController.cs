using ModellClientLib.Context;
using ModellClientLib.Models;
using ModellClientLib.Mqtt;
using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json;
using System.Threading.Tasks;

namespace ModellClientLib.Controllers
{
    public static class BlockController
    {
        public static event Action OnDataChanged;


        public static List<BlockArea> GetBlocks()
        {
            using (var db = new ModellDBContext())
            {
                return db.BlockAreas.AsNoTracking().ToList();
            }
        }

        public static BlockArea GetByID(int id)
        {
            using (var db = new ModellDBContext())
            {
                return db.BlockAreas.FirstOrDefault(b => b.ID == id);
            }
        }


        public static async Task AddBlockAsync(BlockArea block)
        {
            using (var db = new ModellDBContext())
            {
                if (block.ID == 0 && db.BlockAreas.Any())
                {
                    block.ID = db.BlockAreas.Max(b => b.ID) + 1;
                }

                db.BlockAreas.Add(block);
                await db.SaveChangesAsync();
            }

            // 1. Notify UI
            OnDataChanged?.Invoke();

            // 2. Send Config to ESP32 (MQTT)
            string topic = $"{block.ControllerTopic}/config/add_block";

            var payload = new
            {
                id = block.ID,
                type = block.Type.ToString(),
                pinA = block.PinA,
                pinB = block.PinB
            };

            // Fire and forget MQTT
            await MqttClientSingleton.Instance.Transmit(topic, JsonSerializer.Serialize(payload));
        }

        public static async Task UpdateStateAsync(int blockId, bool occupied, string direction, int count)
        {
            using (var db = new ModellDBContext())
            {
                var block = await db.BlockAreas.FindAsync(blockId);
                if (block != null)
                {
                    block.IsOccupied = occupied;
                    block.MagnetCount = count;

                    if (Enum.TryParse(direction, true, out BlockDirection dir))
                    {
                        block.Direction = dir;
                    }

                    await db.SaveChangesAsync();
                }
            }
            OnDataChanged?.Invoke();
        }

        public static async Task ResetBlockAsync(int blockId)
        {
            BlockArea block;
            using (var db = new ModellDBContext())
            {
                block = await db.BlockAreas.FindAsync(blockId);
                if (block != null)
                {
                    // Reset Local State
                    block.IsOccupied = false;
                    block.MagnetCount = 0;
                    block.Direction = BlockDirection.None;

                    await db.SaveChangesAsync();
                }
            }

            if (block != null)
            {
                OnDataChanged?.Invoke();

                string topic = $"{block.ControllerTopic}/control/block_update";
                var payload = new { id = block.ID, cmd = "reset" };

                await MqttClientSingleton.Instance.Transmit(topic, JsonSerializer.Serialize(payload));
            }
        }

        public static async Task FlipDirectionAsync(int blockId)
        {
            BlockArea block;
            using (var db = new ModellDBContext())
            {
                block = await db.BlockAreas.FindAsync(blockId);
                if (block != null)
                {
                    // Toggle Direction Locally
                    if (block.Direction == BlockDirection.Forward)
                        block.Direction = BlockDirection.Reverse;
                    else if (block.Direction == BlockDirection.Reverse)
                        block.Direction = BlockDirection.Forward;

                    await db.SaveChangesAsync();
                }
            }

            if (block != null)
            {
                OnDataChanged?.Invoke();

                string topic = $"{block.ControllerTopic}/control/block_update";
                var payload = new { id = block.ID, cmd = "swap_direction", new_dir = block.Direction.ToString() };

                await MqttClientSingleton.Instance.Transmit(topic, JsonSerializer.Serialize(payload));
            }
        }

        public static async Task DeleteBlockAsync(int blockId)
        {
            using (var db = new ModellDBContext())
            {
                var block = await db.BlockAreas.FindAsync(blockId);
                if (block != null)
                {
                    db.BlockAreas.Remove(block);
                    await db.SaveChangesAsync();
                }
            }
            OnDataChanged?.Invoke();
        }
    }
}