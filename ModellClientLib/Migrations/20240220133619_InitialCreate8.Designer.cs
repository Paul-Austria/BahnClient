﻿// <auto-generated />
using System;
using Microsoft.EntityFrameworkCore;
using Microsoft.EntityFrameworkCore.Infrastructure;
using Microsoft.EntityFrameworkCore.Migrations;
using Microsoft.EntityFrameworkCore.Storage.ValueConversion;
using ModellClientLib.Context;

#nullable disable

namespace ModellClientLib.Migrations
{
    [DbContext(typeof(ModellDBContext))]
    [Migration("20240220133619_InitialCreate8")]
    partial class InitialCreate8
    {
        protected override void BuildTargetModel(ModelBuilder modelBuilder)
        {
#pragma warning disable 612, 618
            modelBuilder.HasAnnotation("ProductVersion", "6.0.24");

            modelBuilder.Entity("ModellClientLib.Models.Segment", b =>
                {
                    b.Property<int>("iD")
                        .ValueGeneratedOnAdd()
                        .HasColumnType("INTEGER");

                    b.Property<string>("SegmentName")
                        .IsRequired()
                        .HasColumnType("TEXT");

                    b.Property<bool>("isActivated")
                        .HasColumnType("INTEGER");

                    b.HasKey("iD");

                    b.ToTable("segments");
                });

            modelBuilder.Entity("ModellClientLib.Models.Servo", b =>
                {
                    b.Property<int>("iD")
                        .ValueGeneratedOnAdd()
                        .HasColumnType("INTEGER");

                    b.Property<int>("MaxDegree")
                        .HasColumnType("INTEGER");

                    b.Property<int>("MinDegree")
                        .HasColumnType("INTEGER");

                    b.Property<int?>("SegmentiD")
                        .HasColumnType("INTEGER");

                    b.Property<int>("degree")
                        .HasColumnType("INTEGER");

                    b.Property<string>("name")
                        .IsRequired()
                        .HasColumnType("TEXT");

                    b.Property<int>("pwmPinId")
                        .HasColumnType("INTEGER");

                    b.Property<string>("topic")
                        .IsRequired()
                        .HasColumnType("TEXT");

                    b.HasKey("iD");

                    b.HasIndex("SegmentiD");

                    b.ToTable("servos");
                });

            modelBuilder.Entity("ModellClientLib.Models.Switch", b =>
                {
                    b.Property<int>("iD")
                        .ValueGeneratedOnAdd()
                        .HasColumnType("INTEGER");

                    b.Property<bool>("CoSwitch")
                        .HasColumnType("INTEGER");

                    b.Property<int?>("SegmentiD")
                        .HasColumnType("INTEGER");

                    b.Property<string>("name")
                        .IsRequired()
                        .HasColumnType("TEXT");

                    b.Property<int>("pinNumber")
                        .HasColumnType("INTEGER");

                    b.Property<bool>("state")
                        .HasColumnType("INTEGER");

                    b.Property<string>("topic")
                        .IsRequired()
                        .HasColumnType("TEXT");

                    b.HasKey("iD");

                    b.HasIndex("SegmentiD");

                    b.ToTable("switches");
                });

            modelBuilder.Entity("ModellClientLib.Models.Servo", b =>
                {
                    b.HasOne("ModellClientLib.Models.Segment", null)
                        .WithMany("servos")
                        .HasForeignKey("SegmentiD");
                });

            modelBuilder.Entity("ModellClientLib.Models.Switch", b =>
                {
                    b.HasOne("ModellClientLib.Models.Segment", null)
                        .WithMany("switchs")
                        .HasForeignKey("SegmentiD");
                });

            modelBuilder.Entity("ModellClientLib.Models.Segment", b =>
                {
                    b.Navigation("servos");

                    b.Navigation("switchs");
                });
#pragma warning restore 612, 618
        }
    }
}
