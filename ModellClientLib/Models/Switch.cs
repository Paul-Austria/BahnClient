using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ModellClientLib.Models
{
    public class Switch
    {
        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int iD { get; set; }
        [Required]
         public int pinNumber { get; set; }
        [Required]
        public string name { get; set; } = "";
        public bool state { get; set; } = false;
        public bool CoSwitch { get; set; } = true;

        public string topic { get; set; } = "";
    }
}
