using System;
using System.ComponentModel.DataAnnotations;

namespace ModellClientLib.Models
{
    public class Module
    {
        [Key]
        public string ControllerTopic { get; set; }

        public string Description { get; set; } = "New Controller";

        public DateTime LastSeen { get; set; } = DateTime.MinValue;
        public bool IsOnline => DateTime.Now < LastSeen.AddSeconds(30);
    }
}