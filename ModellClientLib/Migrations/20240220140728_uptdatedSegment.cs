using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace ModellClientLib.Migrations
{
    public partial class uptdatedSegment : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropForeignKey(
                name: "FK_servos_segments_SegmentiD",
                table: "servos");

            migrationBuilder.DropForeignKey(
                name: "FK_switches_segments_SegmentiD",
                table: "switches");

            migrationBuilder.DropIndex(
                name: "IX_switches_SegmentiD",
                table: "switches");

            migrationBuilder.DropIndex(
                name: "IX_servos_SegmentiD",
                table: "servos");

            migrationBuilder.DropColumn(
                name: "SegmentiD",
                table: "switches");

            migrationBuilder.DropColumn(
                name: "SegmentiD",
                table: "servos");

            migrationBuilder.CreateTable(
                name: "ServoState",
                columns: table => new
                {
                    ServoID = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    ServoPos = table.Column<int>(type: "INTEGER", nullable: false),
                    SegmentiD = table.Column<int>(type: "INTEGER", nullable: true)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_ServoState", x => x.ServoID);
                    table.ForeignKey(
                        name: "FK_ServoState_segments_SegmentiD",
                        column: x => x.SegmentiD,
                        principalTable: "segments",
                        principalColumn: "iD");
                });

            migrationBuilder.CreateTable(
                name: "SwitchState",
                columns: table => new
                {
                    SwitchID = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    reqSwichState = table.Column<bool>(type: "INTEGER", nullable: false),
                    SegmentiD = table.Column<int>(type: "INTEGER", nullable: true)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_SwitchState", x => x.SwitchID);
                    table.ForeignKey(
                        name: "FK_SwitchState_segments_SegmentiD",
                        column: x => x.SegmentiD,
                        principalTable: "segments",
                        principalColumn: "iD");
                });

            migrationBuilder.CreateIndex(
                name: "IX_ServoState_SegmentiD",
                table: "ServoState",
                column: "SegmentiD");

            migrationBuilder.CreateIndex(
                name: "IX_SwitchState_SegmentiD",
                table: "SwitchState",
                column: "SegmentiD");
        }

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "ServoState");

            migrationBuilder.DropTable(
                name: "SwitchState");

            migrationBuilder.AddColumn<int>(
                name: "SegmentiD",
                table: "switches",
                type: "INTEGER",
                nullable: true);

            migrationBuilder.AddColumn<int>(
                name: "SegmentiD",
                table: "servos",
                type: "INTEGER",
                nullable: true);

            migrationBuilder.CreateIndex(
                name: "IX_switches_SegmentiD",
                table: "switches",
                column: "SegmentiD");

            migrationBuilder.CreateIndex(
                name: "IX_servos_SegmentiD",
                table: "servos",
                column: "SegmentiD");

            migrationBuilder.AddForeignKey(
                name: "FK_servos_segments_SegmentiD",
                table: "servos",
                column: "SegmentiD",
                principalTable: "segments",
                principalColumn: "iD");

            migrationBuilder.AddForeignKey(
                name: "FK_switches_segments_SegmentiD",
                table: "switches",
                column: "SegmentiD",
                principalTable: "segments",
                principalColumn: "iD");
        }
    }
}
