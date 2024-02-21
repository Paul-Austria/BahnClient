
using ModellClientLib.Models;
using Microsoft.EntityFrameworkCore;
using System;
using System.Collections.Generic;

namespace ModellClientLib.Context
{
    public class ModellDBContext : DbContext
    {
        public string DbPath { get; }

        public ModellDBContext() 
        {
            var folder = Environment.SpecialFolder.LocalApplicationData;
            var path = Environment.GetFolderPath(folder);
            DbPath = System.IO.Path.Join(path, "test.db");
        }

        protected override void OnConfiguring(DbContextOptionsBuilder options)
      => options.UseSqlite($"Data Source={DbPath}");

        public DbSet<Switch> switches { get; set; }
        public DbSet<Servo> servos { get; set; }
        public DbSet<Segment> segments { get; set; }

    }
}
