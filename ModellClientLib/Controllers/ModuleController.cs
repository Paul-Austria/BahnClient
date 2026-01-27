using ModellClientLib.Context;
using ModellClientLib.Models;
using Microsoft.EntityFrameworkCore;

namespace ModellClientLib.Controllers
{
    public static class ModuleController
    {
        public static event Action OnDataChanged;

        public static List<Module> GetModules()
        {
            using (var db = new ModellDBContext())
            {
                return db.Modules.AsNoTracking().ToList();
            }
        }

        public static async Task UpdateHeartbeatAsync(string controllerTopic)
        {
            using (var db = new ModellDBContext())
            {
                var module = await db.Modules.FindAsync(controllerTopic);

                if (module == null)
                {
                    // Auto-register new module
                    module = new Module
                    {
                        ControllerTopic = controllerTopic,
                        LastSeen = DateTime.Now
                    };
                    db.Modules.Add(module);
                }
                else
                {
                    module.LastSeen = DateTime.Now;
                }

                await db.SaveChangesAsync();
            }
            OnDataChanged?.Invoke();
        }

        public static List<BlockArea> GetBlocksForModule(string controllerTopic)
        {
            using (var db = new ModellDBContext())
            {
                return db.BlockAreas.AsNoTracking()
                        .Where(b => b.ControllerTopic == controllerTopic)
                        .ToList();
            }
        }
    }
}