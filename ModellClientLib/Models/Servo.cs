using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ModellClientLib.Models
{
    public class Servo
    {
        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int iD { get; set; }

        public string topic { get; set; } = "";
        [Required]
        public string name { get; set; } = "";

        public int pwmPinId { get; set; } = 0;

        public int MinDegree { get; set; } = 0;

        public int MaxDegree { get; set; } = 0;

        [Required]
        private int currentDegree = 0;
        public int degree { get { return currentDegree; } set {
                if(value >= MinDegree && value <= MaxDegree)
                {
                    currentDegree = value;
                }    
            
            }
        }

    }
}
