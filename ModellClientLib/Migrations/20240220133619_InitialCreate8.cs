using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace ModellClientLib.Migrations
{
    public partial class InitialCreate8 : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.RenameColumn(
                name: "Degree",
                table: "servos",
                newName: "degree");

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

            migrationBuilder.CreateTable(
                name: "segments",
                columns: table => new
                {
                    iD = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    SegmentName = table.Column<string>(type: "TEXT", nullable: false),
                    isActivated = table.Column<bool>(type: "INTEGER", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_segments", x => x.iD);
                });

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

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropForeignKey(
                name: "FK_servos_segments_SegmentiD",
                table: "servos");

            migrationBuilder.DropForeignKey(
                name: "FK_switches_segments_SegmentiD",
                table: "switches");

            migrationBuilder.DropTable(
                name: "segments");

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

            migrationBuilder.RenameColumn(
                name: "degree",
                table: "servos",
                newName: "Degree");
        }
    }
}
