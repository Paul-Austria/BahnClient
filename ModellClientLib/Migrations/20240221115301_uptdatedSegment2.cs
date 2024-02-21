using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace ModellClientLib.Migrations
{
    public partial class uptdatedSegment2 : Migration
    {
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropPrimaryKey(
                name: "PK_SwitchState",
                table: "SwitchState");

            migrationBuilder.DropPrimaryKey(
                name: "PK_ServoState",
                table: "ServoState");

            migrationBuilder.AlterColumn<int>(
                name: "SwitchID",
                table: "SwitchState",
                type: "INTEGER",
                nullable: false,
                oldClrType: typeof(int),
                oldType: "INTEGER")
                .OldAnnotation("Sqlite:Autoincrement", true);

            migrationBuilder.AddColumn<int>(
                name: "SwitchStateID",
                table: "SwitchState",
                type: "INTEGER",
                nullable: false,
                defaultValue: 0)
                .Annotation("Sqlite:Autoincrement", true);

            migrationBuilder.AlterColumn<int>(
                name: "ServoID",
                table: "ServoState",
                type: "INTEGER",
                nullable: false,
                oldClrType: typeof(int),
                oldType: "INTEGER")
                .OldAnnotation("Sqlite:Autoincrement", true);

            migrationBuilder.AddColumn<int>(
                name: "ServoStateID",
                table: "ServoState",
                type: "INTEGER",
                nullable: false,
                defaultValue: 0)
                .Annotation("Sqlite:Autoincrement", true);

            migrationBuilder.AddPrimaryKey(
                name: "PK_SwitchState",
                table: "SwitchState",
                column: "SwitchStateID");

            migrationBuilder.AddPrimaryKey(
                name: "PK_ServoState",
                table: "ServoState",
                column: "ServoStateID");
        }

        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropPrimaryKey(
                name: "PK_SwitchState",
                table: "SwitchState");

            migrationBuilder.DropPrimaryKey(
                name: "PK_ServoState",
                table: "ServoState");

            migrationBuilder.DropColumn(
                name: "SwitchStateID",
                table: "SwitchState");

            migrationBuilder.DropColumn(
                name: "ServoStateID",
                table: "ServoState");

            migrationBuilder.AlterColumn<int>(
                name: "SwitchID",
                table: "SwitchState",
                type: "INTEGER",
                nullable: false,
                oldClrType: typeof(int),
                oldType: "INTEGER")
                .Annotation("Sqlite:Autoincrement", true);

            migrationBuilder.AlterColumn<int>(
                name: "ServoID",
                table: "ServoState",
                type: "INTEGER",
                nullable: false,
                oldClrType: typeof(int),
                oldType: "INTEGER")
                .Annotation("Sqlite:Autoincrement", true);

            migrationBuilder.AddPrimaryKey(
                name: "PK_SwitchState",
                table: "SwitchState",
                column: "SwitchID");

            migrationBuilder.AddPrimaryKey(
                name: "PK_ServoState",
                table: "ServoState",
                column: "ServoID");
        }
    }
}
