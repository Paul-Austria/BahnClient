using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations.Schema;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ModellClientLib.Models
{
    public class Segment
    {
        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int iD { get; set; }
        public string SegmentName { get; set; } = "";
        public string SegmentGroup { get; set; } = ""; 
        public bool isActivated { get; set; } = false;
        public ICollection<ServoState>  servos { get; set; } = new List<ServoState>();
        public ICollection<SwitchState> switchs { get; set; } = new List<SwitchState>();
    }


    public class ServoState
    {
        public enum MinMax
        {
            Min,Max
        }

        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int ServoStateID { get; set; }
        public int ServoID { get; set; }
        public MinMax ServoPos { get; set; } 
    }

    public class SwitchState
    {
        [Key]
        [DatabaseGenerated(DatabaseGeneratedOption.Identity)]
        public int SwitchStateID { get; set; }
        public int SwitchID { get; set; }
        public bool reqSwichState { get; set; }
    }
}
