using Microsoft.EntityFrameworkCore;
using ModellClientLib.Context;
using ModellClientLib.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ModellClientLib.Controllers
{
    public static class SegmentController
    {
        public static void AddSegment(Segment segment)
        {
            var dbContext = new ModellDBContext();
            dbContext.segments.Add(segment);
            dbContext.SaveChanges();

        }
        public static void RemoveSegment(Segment segment)
        {
            using (var dbContext = new ModellDBContext())
            {
                // Load the segment with its related entities
                segment = dbContext.segments
                    .Include(s => s.switchs)
                    .Include(s => s.servos)
                    .Single(s => s.iD == segment.iD);


                // Remove the segment itself
                dbContext.segments.Remove(segment);
                dbContext.SaveChanges();
            }
        }

        public static Segment GetSegment(int ID) {
            var dbContext = new ModellDBContext();
            var sw = dbContext.segments.Include(s => s.switchs)
                    .Include(s => s.servos).Single(s => s.iD == ID);
            return sw;
        }


        public static ICollection<Segment> GetSegments()
        {
            var dbContext = new ModellDBContext();
            return dbContext.segments.ToList();
        }

        public static async Task<bool> activateSegmentAsync(int ID)
        {
            
            var dbContext = new ModellDBContext();

            var segment = GetSegment(ID);
            if (segment != null) {
                segment.isActivated = true; 
                foreach (var sw in segment.switchs) {
                    Switch cSW = SwitchController.GetByID(sw.SwitchID);
                    if (cSW != null)
                    {
                        cSW.state = sw.reqSwichState;
                        await SwitchController.UpdateSWAsync(cSW);
                    }
                }

                foreach(var serv in segment.servos)
                {
                    Servo servo = ServoController.GetByID(serv.ServoID);
                    if(servo != null)
                    {
                        if (serv.ServoPos == ServoState.MinMax.Min) servo.degree = servo.MinDegree;
                        else servo.degree = servo.MaxDegree;
                        await ServoController.UpdateServoAsync(servo);
                    }
                }

                dbContext.segments.Update(segment);
                dbContext.SaveChanges();
                DeactiveOtherSegment(segment);
                return true;
            }
            return false;

        }

        /// <summary>
        /// Deactives every Segment that has the same elements as the provided segment
        /// </summary>
        /// <param name="segment">Current Segment</param>
        private static void DeactiveOtherSegment(Segment segment)
        {
            var dbContext = new ModellDBContext();


            var segments = dbContext.segments
                .Include(s => s.switchs)
                .Include(s => s.servos).Where(s => s.iD != segment.iD && s.isActivated && s.SegmentGroup == segment.SegmentGroup);

            foreach (var segTOCheck in segments)
            {
                var ret = segTOCheck.servos.Where(s => segment.servos.Any(sn => sn.ServoID == s.ServoID));
                if (ret.Count() != 0) segTOCheck.isActivated = false;
                var retSW = segTOCheck.switchs.Where(s => segment.switchs.Any(sn => sn.SwitchID == s.SwitchID));
                if (retSW.Count() != 0) segTOCheck.isActivated = false;
                dbContext.segments.Update(segTOCheck);
            }
            dbContext.SaveChanges();
        }
            
    }
}
