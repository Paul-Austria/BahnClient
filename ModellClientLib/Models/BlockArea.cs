using System.ComponentModel.DataAnnotations;
using System.Text.Json.Serialization;

namespace ModellClientLib.Models
{
    [JsonConverter(typeof(JsonStringEnumConverter))]
    public enum BlockType
    {
        Magnet,         // 2-Pin (Entry/Exit)
        CurrentSensor,   // 1-Pin (Occupancy only)
        OnePointCurrentSensor // 1-Pin with special handling meant to fire events
    }

    [JsonConverter(typeof(JsonStringEnumConverter))]
    public enum BlockDirection
    {
        None,
        Forward,        // A -> B
        Reverse         // B -> A
    }

    public class BlockArea
    {
        [Key]
        public int ID { get; set; }

        public string Name { get; set; } = "";
        public string ControllerTopic { get; set; } = "layout/default";

        public BlockType Type { get; set; } = BlockType.Magnet;

        public int PinA { get; set; }     
        public int? PinB { get; set; }    

        public bool IsOccupied { get; set; }
        public BlockDirection Direction { get; set; } = BlockDirection.None;
        public int MagnetCount { get; set; }
    }
}