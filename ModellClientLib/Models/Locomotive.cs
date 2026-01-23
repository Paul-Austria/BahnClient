using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;
using System.Text.Json.Serialization;

namespace ModellClientLib.Models
{
    public class Locomotive
    {
        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int ID { get; set; }

        [Required]
        public string Name { get; set; } = "";

        [Required]
        public int Address { get; set; }

        public int Speed { get; set; } = 0;

        // 0 = Reverse, 1 = Forward
        public int Direction { get; set; } = 1;

        public string Topic { get; set; } = "trains/command"; 

        public List<LocoFunction> Functions { get; set; } = new List<LocoFunction>();
    }

    public class LocoFunction
    {
        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int ID { get; set; }

        public int FIndex { get; set; } 
        public string Name { get; set; } = "Func";
        public bool IsActive { get; set; }

        // Foreign Key
        public int LocomotiveID { get; set; }

        [JsonIgnore]
        [ForeignKey("LocomotiveID")]
        public Locomotive Locomotive { get; set; }
    }
}